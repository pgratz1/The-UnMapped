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
- Status: accepted (merged into design.md, 2026-11-18)

### Climate's latitude span is a fraction of the map

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (Climate: "temperature from latitude"; "scale is compressed")
- Problem: a 16 km map spans no real latitude. A gradient per kilometre would give an 8 km world almost no climate variety and a 32 km world four times as much.
- Proposal: the north-to-south temperature difference is a tuning value per map (a map-fraction quantity), the same at every map size, standing for the larger land the map represents.
- Interim choice: recorded in p1-00 section 2.2 for the climate spec (second pass).
- Status: accepted (merged into design.md, 2026-11-18)

### Generator tuning is data, and shipped tuning changes bump the version

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 10 ("Content rules live in TOML data files under game/data/ and worldgen config"), 4 (generator version)
- Problem: the design says tuning lives in TOML but not where, how it is identified in a world, or whether changing it is a version change.
- Proposal: tuning is a data kind `worldgen` in `game/data/worldgen/`, one definition per stage, every field required, covered by `content_hash`. A change to the shipped data bumps the PATCH version and re-records the shipped-data golden hashes; algorithm golden tests use a frozen fixture copy so tuning work does not churn them.
- Interim choice: as proposed, in p1-00 section 6 (needs P0-03 amendment 16 for kinds with a schema per id).
- Status: accepted (merged into design.md, 2026-11-18)

### Rivers are rasters in P1; a river table later if history needs one

- See P1-02 section 9 (the entry is written there in full).
- Status: accepted (merged into design.md, 2026-11-18)

### Elevation is final after the elevation stage; hydrology does not carve

- See P1-02 section 9.
- Status: accepted (merged into design.md, 2026-11-18)

### Biome categories and the biome table

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 4 (six starting biomes; special cases for coast, river and high peaks), 6 (`region.biome`)
- Problem: the biome layer needs codes for water cells; the design does not say whether coast, river and peak special cases are separate biomes; a world file does not say what its biome codes mean.
- Proposal: biome definitions carry a fixed integer `code` and a colour. Categories: the six land biomes plus `ocean` and `lake`. River cells take the biome of their land; coast and peak special cases choose among the six. `blighted_waste` is assigned only by history. The biome stage writes a `biome` table (code, key, name, colour, water) into the world (schema version 2).
- Interim choice: as proposed, in p1-00 section 6.6 and 9.2.
- Status: accepted (merged into design.md, 2026-11-18)

### The layer catalogue is part of the schema contract

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (schema rules; "a schema reference lives at docs/specs/schema.md")
- Problem: layers are rows of the `layer` table, so the DDL does not change when a layer is added, yet a reader depends on each layer's type and units.
- Proposal: the layer catalogue (name, type, units, meaning) is part of the contract recorded in `schema.md`. Adding a layer needs no schema version bump; changing a catalogued layer does.
- Interim choice: as proposed, in p1-00 section 4 rule 3.
- Status: accepted (merged into design.md, 2026-11-18)

### P1 schema changes (one entry per phase)

- Date: 2026-11-15
- Raised by: spec-writer
- Design doc section: 6 (schema growth)
- Problem: the phase's schema changes need one proposals entry (P0-02 5.2 rule 3).
- Proposal: first pass (elevation, hydrology): none; schema stays at version 1. Second pass: version 2 adds table `biome`; version 3 (or the same bump, if in one branch) adds table `region`. Updated 2026-11-16 with the second-pass specs: version 2 adds table `biome` (P1-04); version 3 adds table `region`, with `bounds` stored as four integer columns `x_min`, `y_min`, `x_max`, `y_max` because tables hold no blobs, and a new table `region_border` (border length, lowest crossing, walkable and river flags per pair of neighbouring regions; P1-06), which is how history finds where a pass or a bridge is worth building. The layer catalogue gains `walk_area` (connected walkable ground; P1-05).
- Interim choice: as proposed, in p1-00 section 9.
- Status: accepted (merged into design.md, 2026-11-18)

## Phase P2 Godot bring-up and viewer v0 (2026-11-15)

### Godot extension runs from a copy of the project in the build tree

- Date: 2026-11-15
- Raised by: viewer-engineer
- Design doc section: 10 (build output stays outside the source tree; repository layout)
- Problem: Godot writes its cache (`.godot/`) inside the project and loads a GDExtension from a path relative to the project; the cache location cannot be configured. With `game/` in a synced folder, opening it directly writes `.godot/` into the repository, and a link from `game/` to the build tree would be machine-specific and trip CI's clean-checkout check.
- Proposal: state in section 10 that Godot is always run on a copy of the project assembled under the build tree (`<build>/game/`: links to `game/`'s entries, the extension's output in `bin/`, Godot's `.godot/` cache), and that `game/` is never opened directly.
- Interim choice: implemented (`gdext/cmake/assemble_project.cmake`); `CLAUDE.md` and `gdext/README.md` describe it. Saving `project.godot` from the editor with safe save breaks its link; the build warns.
- Status: accepted (merged into design.md, 2026-11-18)

### Pinned engine: Godot 4.7.2 and godot-cpp 10.0.0

- Date: 2026-11-15
- Raised by: viewer-engineer
- Design doc section: 10 (Engine row: "pinned in the repo to the newest stable version that Terrain3D supports")
- Problem: the design gives the rule but not the version.
- Proposal: record Godot 4.7.2-stable (Terrain3D 1.1.0 states "Supports Godot 4.5-4.8+"; 4.8 was at beta when pinned) and godot-cpp 10.0.0-stable targeting GDExtension API 4.7, vendored in `extern/godot-cpp/`. The extension is an optional part of the build (`UNMAPPED_BUILD_GDEXT`, off by default) so `worldgen` and `worldgen-cli` never need Godot.
- Interim choice: as proposed; evidence and checksums in `gdext/README.md`, `extern/README.md` and `tools/ci/godot-4.7.2-stable.sha512`.
- Status: accepted (merged into design.md, 2026-11-18)

### Friction writing Godot code purely in C++ (P2 findings)

- Date: 2026-11-15
- Raised by: viewer-engineer
- Design doc section: 3, 8, 10 (C++ only, no GDScript)
- Problem: observed while building viewer v0 with godot-cpp 10 and Godot 4.7.2: (1) every method, signal and property visible to Godot needs hand-written registration, and scene-to-code wiring is by node name, checked only at run time; (2) iteration is rebuild then restart, with no hot reload, and godot-cpp adds about 1,100 files and three minutes to a clean build; (3) godot-cpp's defaults clash with a normal C++ library (exceptions off, static MSVC runtime, files written into the source tree) and each needed an override; (4) headless testing has traps: Godot idles instead of exiting if the extension fails to load, and the headless display has no window size, so layouts collapse; (5) Godot changes the working directory to the project, so relative command-line paths break; (6) Godot's integer is 64-bit signed, so unsigned 64-bit layer cells are shown as text; (7) a layout bug was invisible to a headless pixel check and found only in a windowed run; (8) sanitizer builds of the extension are impossible against the stock editor.
- Proposal: keep C++ only, and plan for it: every screen gets a headless check scene from the start, as the viewer has; section 10's "headless Godot runs for scene tests" becomes one CTest per screen, run where the pinned editor is available; budget CI time and a compiler cache for godot-cpp; revisit hot reload when gameplay iteration starts.
- Interim choice: all handled in `gdext/` and `extern/CMakeLists.txt`.
- Status: accepted (merged into design.md, 2026-11-18)

### Walkable terrain: slope and flatness as acceptance criteria

- Date: 2026-11-15
- Raised by: world critic and spec-writer, via the lead
- Design doc section: 4 (geography), 7 (runtime terrain), 8 (first person in both modes)
- Problem: the design gives no target for how steep terrain may be. The first accepted-looking elevation candidate had mountain land with a median slope of 45 to 60 degrees: every range was a wall a first-person player cannot walk up, and neither the images nor the existing step statistics showed it.
- Proposal: state in section 4 that terrain must be walkable at 16 m cells: cliffs and crags are features, not whole mountainsides. Acceptance is by slope statistics in `report` (share of land steeper than 35 and 45 degrees, median slope of high ground, share of flat lowland), with bounds set from real terrain. Consequence: on a 16 km continent peaks are about 900 to 1,500 m, not 2,000 m and more, in keeping with the compressed scale of section 4.
- Interim choice: the elevation design was reworked to these targets (candidate v14) and Paul preferred the earlier, steeper candidate; see "High peaks may be inaccessible; history opens the passes" below. The review is in docs/reviews/p1-01-elevation-v13.md.
- Status: superseded

### Every world has real uplands

- Date: 2026-11-15
- Raised by: world critic, via the lead
- Design doc section: 4, 5
- Problem: about a quarter of generated continents had almost no mountains (one small massif on an undivided plain), which leaves dwarves nowhere to live and history no natural borders or passes.
- Proposal: require, by construction in the elevation stage, that every world has a minimum share of high ground forming at least one range. Do not relax the statistics to admit mountain-light worlds.
- Interim choice: adopted as a requirement for the elevation redesign.
- Status: accepted (merged into design.md, 2026-11-18)

### High peaks may be inaccessible; history opens the passes

- Date: 2026-11-15
- Raised by: Paul, at the review of the elevation candidates
- Design doc section: 4 (geography), 5 (history), 7 (runtime terrain)
- Problem: making all mountain land walkable (candidate v14) turned every continent into one smooth central massif. The candidate with long, dramatic ranges (v13) has high ground too steep to walk.
- Proposal: keep the dramatic ranges. Some areas of a map may be unwalkable, at least initially: high peaks and steep range flanks are barriers. Civilizations forge roads and passes through such regions over time, as part of the history simulation, so what the player can reach in the present day depends on what history built. Geography reports how much land is steep and whether natural passes exist; it does not require mountains to be walkable.
- Interim choice: the elevation stage follows v13 with somewhat more erosion (P1-01); walkability statistics in `report` become descriptions, not limits. This supersedes the entry "Walkable terrain: slope and flatness as acceptance criteria" as a requirement; the statistics stay.
- Status: accepted (merged into design.md, 2026-11-18)

### Natural passes through the ranges

- Date: 2026-11-15
- Raised by: world critic, via the lead
- Design doc section: 4 (geography), 5 (history)
- Problem: in the adopted elevation candidate the ranges are closed walls with open ends. Measured at 1024, a median of 1.3% of land above 500 m can be reached on foot, no walkable route climbs above about 250 m, and walkers go round a range (detours of 1.5 to 3 times, up to 10 times). There are no saddle passes, so history has no natural crossing to fortify, tax or fight over; every crossing must be a built road.
- Proposal: decide whether ranges should have a few natural passes. A cheap way: one more low-frequency noise lowers the mountains only where its zero line crosses a belt, giving one to three cols 500 to 800 m wide per range and leaving crests elsewhere alone. It changes every world's bytes but no outline or range layout.
- Interim choice: Paul decided on 2026-11-17 to add natural passes, together with the deferred fix for very steep smooth faces where a range meets the sea (P1-01 limitation 11), as one elevation revision. Until it merges: no passes in P1-01 as first merged. Low coastal cliffs also seal off a few lowland pockets (for example 12 km² on 1024 seed 18); they are left as they are.
- Update 2026-11-17, after five exploration rounds and two critic reviews (specified in P1-01, generator 0.6.0): passes sit at true saddles of each range's drainage divide and are a lowering of the range's envelope before erosion, so erosion and rivers shape them like any other ground. The cut at a col is capped by one tuning value (500 m, chosen by Paul on 2026-11-17 after comparing 400, 500 and 600; 400 to 600 m is the range that still looks natural), so pass height follows the terrain: naturally low saddles become low gates, others stay high cols. Walkability is an outcome, not a requirement: at 500 m about two passes in five are walkable end to end. Paul accepts this as realistic: history may improve a pass, and a player character with magic may find another way over. The steepest-step bound is 150 m at every map size again.
- Update 2026-11-17, after the merge of generator 0.6.0: Paul looked at the passes and accepts them as they are. The critic's follow-ups (low gates that cut a range nearly to the plain, a mountain lake sometimes erased on a crossing) are not to be pursued now; they stay listed in docs/reviews/p1-01b-elevation-passes-critic.md.
- Status: accepted (merged into design.md, 2026-11-18)

### Every world's hash depends on the shipped tuning files

- Date: 2026-11-16
- Raised by: generator-engineer, via the lead
- Design doc section: 10, 13 (determinism, golden hashes)
- Problem: once the `worldgen` tuning kind is registered (P1-01), the content hash of the loaded data is written into every world, including worlds generated with no geography stage. Any change to a shipped tuning file therefore changes every golden hash, the P0 ones included.
- Proposal: accept it and say so in section 13: a shipped tuning change is an output change, bumps the generator version and re-records all golden hashes (already the rule in "Generator tuning is data, and shipped tuning changes bump the version").
- Interim choice: as proposed; `p0-worlds.txt` was re-recorded with generator version 0.3.0.
- Status: accepted (merged into design.md, 2026-11-18)

### Second-pass geography: choices of taste for Paul

- Date: 2026-11-16
- Raised by: spec-writer, via the lead
- Design doc section: 4 (geography), 5 (history)
- Problem: the climate, biome, resource and region specs (P1-03 to P1-06) had to choose where the design is silent.
- Proposal: confirm or change each. (1) One prevailing wind per world, drawn uniformly from eight directions, so rain shadows differ between worlds. (2) A cool climate: 5 to 9 °C at sea level north to south, 6.5 °C per km of height, so alpine ground starts near 1,000 m. (3) Five natural biomes only: alpine, highland moor, forest, grassland, marsh; blighted waste stays a history overlay. (4) Resource and habitability scores are absolute, not relative to each map, so a poor world is poor. (5) Habitability weights fertility 45, fresh water 30, timber 15, ore 10; history reweights by race. (6) Regions of about 2 km² (roughly 12, 45 and 180 per map at 512, 1024 and 2048), bordered on ridges and large rivers, with placeholder names such as "East Wood II"; history gives cultural names later. (7) No sea regions; lakes belong to land regions.
- Interim choice: as listed; all numeric acceptance ranges in these specs are provisional until the first implementation sweep.
- Status: accepted (merged into design.md, 2026-11-18)

### Pass sites are stored in the world file

- Date: 2026-11-17
- Raised by: spec-writer, via the lead
- Design doc section: 5 (history), 6 (schema growth)
- Problem: history needs to know where the passes are (places to fortify, tax, fight over, build a road), and they cannot be recovered from the stored layers: a pass is a decision of the elevation stage, not a property the height field shows unambiguously.
- Proposal: a table `pass`, one row per pass in site order: range id, kind, whether the cut was capped, the col cell, the natural saddle height and the col height, and the crossing path. This is the first table beyond `meta` and `layer`, so the schema goes to version 2 with generator 0.6.0; the planned `biome` table then takes version 3 and `region` with `region_border` version 4.
- Interim choice: as proposed, specified in P1-01 section 9.1 and P1-00 section 9.3.
- Status: accepted (merged into design.md, 2026-11-18)

### Schema version 3: the biome table and the stored wind direction

- Date: 2026-11-17
- Raised by: spec-writer and world critic, via the lead
- Design doc section: 4 (geography), 6 (schema growth), 7 (runtime)
- Problem: the biome layer stores codes whose meaning lives in the data files, so a world file needs the biome definitions it was generated with; and the prevailing wind of a world, which the runtime will want for weather and vegetation, was computed by the climate stage but not stored.
- Proposal: schema version 3, with generator 0.8.0 (the biome stage). Table `biome` (id, key, name, colour, water flag), one row per biome definition, written by the biome stage. Table `climate` (one row: `wind_toward`, the direction the wind blows toward as one of eight codes), written by the climate stage; the climate layers do not change. Both tables exist in every world, empty when their stage did not run. `region` and `region_border` remain planned as version 4.
- Interim choice: as proposed, specified in P1-04, P1-03 and docs/specs/schema.md.
- Status: accepted (merged into design.md, 2026-11-18)

### Biome thresholds scale with map size

- Date: 2026-11-17
- Raised by: world critic and spec-writer, via the lead
- Design doc section: 4 (geography)
- Problem: rainfall depends on map size (air dries as it crosses land; median land rainfall is about 825, 755 and 680 mm at 512, 1024 and 2048 cells), so one fixed forest threshold made small maps almost all forest and large maps half open country.
- Proposal: each biome rule's rainfall bounds are multiplied by a per-size factor held in the tuning file (1.125, 1, 0.875), so forest starts at about 810, 720 and 630 mm. Thresholds are not set from each world's own rainfall quantiles: that would give every world the same shares and erase the differences that come from where the ranges lie and which way the wind blows. Measured on 108 worlds: forest 27 to 64% and grassland 16 to 59% of land, every biome present in every world, none above 65%. Marsh still grows with map size (about 1, 2.4 and 5.7% median), because larger maps have more flat lowland; accepted.
- Interim choice: as proposed, specified in P1-04.
- Status: accepted (merged into design.md, 2026-11-18)

### More natural biomes, per-world temperature, a thinner coastal forest

- Date: 2026-11-17
- Raised by: world critic (reviews of climate and biomes), decided by Paul
- Design doc section: 4 (geography), 5 (peoples and beasts by biome)
- Problem: with five natural biomes every range had the same rings (bare peak, moor) on its wet and dry sides, grassland was one undivided kind, rivers crossing open country had bare banks, marsh was thin ribbons that never formed a region, every world had the same temperatures, and the first kilometre of every coast was forest even on the dry side.
- Proposal (Paul, 2026-11-17: "add all of them for more variety"; yes to temperature; yes to the coast): four additions to the natural biomes: heath (dry upland on the lee side of a range), steppe (the dry heart of a rain shadow), wet woodland (along rivers through open country), and fens (marsh allowed to form broad wetlands, enough to be a homeland). Each world gets its own overall temperature, about 2 °C either way. The coastal rain bonus is reduced so lee coasts read as dry. The design doc's list of six biomes (five natural and blighted waste) grows accordingly; races, beasts and the runtime's planting should take the new biomes into account.
- Interim choice: to be specified as a revision of P1-03 and P1-04 after the first biome stage merges (generator 0.8.0); calibrated on real worlds and reviewed by the world critic before the spec is fixed.
- Status: accepted (merged into design.md, 2026-11-18)

### Large maps may have unusual worlds

- Date: 2026-11-18
- Raised by: validation specialist, decided by Paul
- Design doc section: 4 (geography)
- Problem: a sweep of twenty worlds at the largest map size found two outside elevation's hard acceptance bounds: one with almost no high ground (3% of land above 500 m; one short coastal range on a broad lowland continent) and one with a summit of 2,931 m against an intended maximum of 2,800 m. The mountain guarantee measures the footprint of the mountain belt, not the height reached, and the height scaling leaves the very top unbounded.
- Proposal (Paul, 2026-11-18): leave it; this is natural variation between worlds. The affected bounds at the largest size become descriptive: an occasional large world is nearly flat or has an unusually tall peak. This qualifies the earlier entry "Every world has real uplands", which still holds at the two smaller sizes.
- Interim choice: no change to elevation; the acceptance ranges for the largest size are widened in the specs with the next revision, and the two worlds stay recorded in docs/reviews/p1-04-biomes-validation.md.
- Status: accepted (merged into design.md, 2026-11-18)

### Schema version 4: the stored per-world temperature offset

- Date: 2026-11-18
- Raised by: spec-writer, via the lead
- Design doc section: 4 (geography), 6 (schema growth), 7 (runtime)
- Problem: each world now has its own overall temperature (Paul's decision of 2026-11-17), which the runtime and the history phase need to know and which cannot be recovered exactly from the temperature layer.
- Proposal: schema version 4, with generator 0.9.0. Table `climate` gains the column `temp_offset_dc` (tenths of a degree, between −20 and +20 with shipped tuning). Table `biome` keeps its shape and has twelve rows (ocean, lake, nine natural biomes, blighted waste). `region` and `region_border` move to version 5.
- Interim choice: as proposed, specified in P1-03, P1-04 (third version) and docs/specs/schema.md.
- Status: accepted (merged into design.md, 2026-11-18)

### Wild food (forage) as a fifth resource score; habitability treats water as a necessity

- Date: 2026-11-18
- Raised by: spec-writer and world critic, approved by the lead
- Design doc section: 4 (geography: resources, habitability), 5 (peoples), 6 (history: homelands, settlement)
- Problem: the resource scores valued land only as farmland, timber or ore, so marsh, fen, wet woodland and coasts were worth nothing, although the design has hunting and fishing feed settlements and wild-land peoples live there. The earlier habitability sketch added water as one bonus among others, so dry ground far from any river could still score well.
- Proposal: a `forage` score (wild food: game, fowl, fish, reeds), from a per-biome value held as data plus a bonus within 300 m of the sea, a lake or a large river. History uses it as a second food source and for peoples of wild places. Habitability is a weighted mean of fertility (55), forage (20), timber (15) and ore (10), multiplied by a fresh-water factor between 0.25 and 1 and reduced by slope, flooding and very small walkable pockets; there is no separate coast bonus, since the sea's value comes through forage. Steppe fertility is 0.7 and "fertile" land is fertility 0.65 or more, so forest (0.7) counts as clearable farmland. All scores are absolute (0 to 255), not normalized per world, so a harsh world reads as harsh. Small maps are mostly mountain: at the smallest size a world may have well under a third of its land walkable in one piece, and that bound is descriptive there. This revises item (5) of "Second-pass geography: choices of taste for Paul".
- Interim choice: as proposed, specified in P1-05 (second version), generator 0.10.0. The stage writes layers only; the schema stays at version 4, the layer catalogue gains `forage`, and `region` becomes layer 19. Weights and per-biome values are tuning data and can be changed without a code change.
- Status: accepted (merged into design.md, 2026-11-18)

### Habitability weights revised after the critic's review (design doc section 4 now out of date)

- Date: 2026-11-18
- Raised by: world critic, approved by the lead
- Design doc section: 4 (geography: resources and habitability)
- Problem: the design doc gives the habitability weights as fertility 55, forage 20, timber 15, ore 10. With those weights forest outscored grassland in every world reviewed, lake shores scored below the land behind them, and at the smallest map size some worlds had no rich ore reachable on foot (`docs/reviews/p1-05-resources-critic.md`).
- Proposal: weights fertility 70, forage 15, timber 10, ore 5; an alluvial fertility bonus of 0.15; a milder flood penalty (factor 0.75 on ground within 0.5 m of the water level); ore exposure that favours lower, less steep ground. The design doc could give the weights as tuning data ("farmland weighs most") instead of fixed numbers.
- Interim choice: as proposed, in spec P1-05 revision r2 and generator 0.10.0 (merged). All of it is tuning data in `game/data/worldgen/resources.toml`.
- Known limits, accepted for now: the banks of large rivers score slightly below their hinterland (wet biomes have low fertility); lowland regions differ little from each other (a fix needs code, for example low-frequency soil variation); coasts score below inland, so ports must come from forage and river mouths in the history phase; rich ore is often on crags nobody can walk to, so history should site mines by "ore near a walk area"; the smallest maps are tight and need homeland fallbacks.
- Status: open

### Paul's decisions of 2026-11-18: heath stripe, fen patches and habitability limits stay as they are

- Date: 2026-11-18
- Raised by: lead, decided by Paul
- Design doc section: 4 (geography: biomes, resources and habitability)
- Problem: two world-critic follow-ups were open. Biomes: heath shows as a stripe along mountain flanks on some worlds, and fens are blocky patches on the largest maps (`docs/reviews/p1-04b-biome-variety-critic.md`). Resources: the limits listed in the entry above (river banks below their hinterland, little contrast between lowlands, habitability falling to zero along the sharp line where mountain flanks begin).
- Proposal: none needed in the design doc.
- Interim choice: Paul decided to leave the heath stripe and the fens as they are, and to accept the habitability limits for now. Soil variation between lowlands is to be reconsidered only if the regions stage or the history phase needs more contrast. The explored alternatives (a height-above-valley term for heath, with or without noise) stay on branch `p1/04c-heath-fen-explore` and are not merged.
- Status: decided

### Schema version 5: regions, region borders and biome counts per region

- Date: 2026-11-18
- Raised by: spec-writer, approved by the lead
- Design doc section: 4 (Regions), 6 (table list, schema versions)
- Problem: the design plans schema version 5 with tables `region` and `region_border`, without their columns. Beast placement and race preferences need the biome mix of a region, not only its dominant biome, and history needs to know where ore can be reached.
- Proposal: schema version 5, with the regions stage, adds `region` (id, name, dominant biome, bounds as four integer columns, seed cell, landmass, cell counts of lake, river, coast and walkable ground, largest river class, main walk area, mean and highest elevation, the means of the six resource scores, the 90th percentile of habitability, and counts of rich ore and of rich ore within 300 m of walkable ground), `region_border` (one row per pair of neighbouring regions: border length, the height and cell of the lowest crossing, walkable and large-river pair counts, largest river class, and the smallest id and number of natural passes crossing it) and `region_biome` (cells of each biome in each region; new, not in the design's table list).
- Interim choice: as proposed, specified in P1-06 and `docs/specs/schema.md`; generator 0.11.0.
- Status: open

### Regions: how borders, large rivers, uplands and counts are settled

- Date: 2026-11-18
- Raised by: spec-writer, approved by the lead
- Design doc section: 4 (Regions)
- Problem: the design says regions are "bordered on ridges and large rivers" and "roughly 12, 45 and 180 per map", but not how borders are found, what a large river is, or what happens to mountain land. A prototype on generator 0.10.0 worlds showed that a climbing cost puts borders along contour lines, not ridges.
- Proposal: (1) borders come from a travel cost that is cheap along valleys and dear over ridges (cost weighted by height above the nearest stream); (2) a large river drains more than about 4 km², and its two banks belong to different regions where both have a settlement site; (3) add "Mountain massifs wider than about a kilometre form upland regions of their own; narrower ranges are divided along their crest between the regions on either side"; (4) counts: with 2 km² regions the measured counts are 13, 49 and 200, since land is about 40% of the map. Either say "about 2 km² each, roughly 13, 50 and 200 per map", or keep 12, 45 and 180 and accept regions of about 2.3 km². Paul's choice. (5) Placeholder names have the form "Northwest Wood II" (sector, biome noun, numeral), built from data; islands of any size are regions.
- Interim choice: as proposed, with 2 km² regions (seed spacing 1200 m, a tuning value), specified in P1-06. Paul decided on 2026-11-18 to keep 2 km² regions and to restate the counts in the design doc as roughly 13, 50 and 200 per map. Revision r2 of the spec, after the world critic's review (`docs/reviews/p1-06-regions-critic.md`), added two rules before the merge: a natural pass is attached to at most one region border, the one its path crosses nearest the col, so that a pass is never counted on two borders; and two more name nouns, "Mere" for a region that is at least half lake and "Fells" for an upland region whose dominant biome is not alpine, heath or highland moor. Merged as generator 0.11.0 on 2026-11-19.
- Status: open (decided by Paul; design doc section 4 to be updated)

### Viewer: layer units and region names come from a copy of the catalogue

- Date: 2026-11-18
- Raised by: viewer-engineer
- Design doc section: 6 (world database), 8 (viewer), 10 (`worldgen` public API)
- Problem: the viewer now shows each cell's value in display units (metres, degrees Celsius, millimetres a year, score out of 255) and the names of its biome and region. worldgen's public API names the palette of a layer but not its unit or meaning, so the viewer restates the layer catalogue of spec P1-00 section 4 (`gdext/src/viewer/layer_catalogue.cpp`); a unit change in the catalogue would not reach the viewer unless someone edits that copy. Likewise worldgen's `RegionRecord` (what `report` reads of table `region`) leaves out the name, so the viewer takes it from column 1 of the raw row after worldgen's `read_region_rows` has checked the rows. Smaller friction from the same work: in godot-cpp 10, `const godot::String + "literal"` is an ambiguous overload in some contexts and every such literal needs wrapping in `godot::String(...)`; and a legend is a hand-built `ItemList` with generated swatch textures, since Godot has no colour-legend control.
- Proposal: worldgen gains a public, read-only layer catalogue (name, unit, decimal scale, special values) beside `registered_palette`, and the region record carries `name`. The create-world screen and the in-game map will need the same information, so it belongs with the data rather than in each screen.
- Interim choice: the viewer keeps its copy of the units of P1-00 4 and reads the name column itself; a layer it does not know is shown under "Other" with its raw value.
- Status: open

### Paul's decision of 2026-11-19: P1 Geography approved; the critic's recommendations accepted

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 4 (geography), 5 (history), 13 (phases)
- Problem: phase P1 ends with Paul's review. The world critic's review of the finished geography (`docs/reviews/p1-06-regions-critic.md`) closes with recommendations for the history phase.
- Proposal: none; a record. Paul read the review, accepted its recommendations and approved P1. For P3 this means: the default map size, 1024, is the reference size for tuning and judging history, since 512 worlds have room for few peoples and suit a short history, not all six races; history locates a pass by its col (`pass.col_x`, `col_y`) and the `region` layer; a border counts as a river border by the share of its length that follows the river, not by any river pair; islets below about 0.3 km² are ignored as homes; homelands in wild uplands are chosen by region and biome, since little of that ground is walkable; ports come from history (forage and river mouths), since coasts score below the interior; lowland borders are political lines.
- Interim choice: the P3 specs follow these points.
- Status: decided

### History: quiet intervals advance in steps so that flare-ups can interrupt them

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (Deep time: quiet intervals, flare-ups)
- Problem: a quiet interval "advances the world in one jump", yet it "can be interrupted" when a tension measure crosses a threshold. A single jump cannot be interrupted part-way, and the design does not say how often tension is checked.
- Proposal: a quiet interval is a sequence of quiet steps (25 years by default, tuning); after each step the tension measure is evaluated and, above the threshold, a flare-up (a short active phase) starts the next year. Consecutive quiet steps without change are written as one summary event spanning them.
- Interim choice: as proposed, in `docs/specs/p3-00-history-overview.md` 4.1 and 4.4.
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### History: the calendar starts at the first elven awakening

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (Deep time, awakening table), 6 (`meta` end year)
- Problem: the design gives years relative to the span ("year 0", "around year 2,100") but does not fix year 0 or the present, nor whether there are months.
- Proposal: years are integers; year 0 is the first elven awakening; the present is year E, the history span (6,000 by default), stored as `meta.history_end_year`; events within a year are ordered by event id. Eras shown to players are derived at runtime.
- Interim choice: as proposed (P3-00 2.1).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### History: P3 creates only the creator gods

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (Gods and the order of the races), 13 (P3 and P5 rows)
- Problem: each awakening is "an event with a god, a year and a place", but the pantheon and god interventions are phase P5.
- Proposal: P3 writes one named creator god per race (with its camp) into table `god`, so that awakening events name their creator; the rest of the pantheon, domains and interventions come with P5, which adds columns with a schema bump. P3 defines the seam for interventions and for the divine script (design 14): an intervention source consulted once per tick, empty in P3; `meta.param.divine_script` records the script's hash or nothing.
- Interim choice: as proposed (P3-00 9.2).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### History: territory is held by whole regions

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (claim a region), 13 (P4 territory overlay by year)
- Problem: the design does not say at what grain territory is held, and P4's timeline must show it at any year cheaply.
- Proposal: a region is held by at most one civilization at a time; table `region_control` stores ownership intervals (region, from year, to year, civilization, causing event), and `settlement_census` the settlement states by year, so a viewer answers any year without replaying the log.
- Interim choice: as proposed (P3-00 6.4).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### History: the event log stores facts, not summaries

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 6 (table `event`: `summary`, `site_id`), 14 (enrichment needs structure)
- Problem: the design's `event` table has a `summary` column, while its rules say that text shown to players is generated at runtime from records and templates. Sites do not exist before P6.
- Proposal: `event` holds kind, year and end year, phase, region, settlement and a salience score; participants with roles, integer values and causes are in `event_participant`, `event_value` and `event_cause`. Sentences are rendered from a catalogue of event kinds with templates (data), in the history report, the viewer and the game. `site_id` is added with the sites in P6. The catalogue of kinds, roles and value keys is part of the schema contract, as the layer catalogue is.
- Interim choice: as proposed (P3-00 6.2, 6.3).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### History: event rules are code with TOML tuning in P3; roads and improved passes wait for P5

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 3 / 5 (event rules as hand-edited TOML; tick loop "build a road"; "Mountains, passes and roads in history"), 13 (P3 row)
- Problem: the design lists event rules among the TOML data and puts road building and pass improvement in the tick loop, while the P3 row of section 13 lists civilizations, settlements, wars and ruin only.
- Proposal: in P3 the rules are code from the stage specs, with every number in TOML tuning (kind `history`) and the report templates in data; whether conditions and effects should become data is decided in P5, once it is clear which rules need to vary. Roads and improved passes come with P5 (table `road`); P3 treats passes as fixed crossings.
- Interim choice: as proposed (P3-00 8.4, 15).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### History: schema versions 6 and 7

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 6 (table list, schema versions)
- Problem: the design's table list is the target for all phases; P3 needs a subset, some tables the list does not have, and the `meta` keys for the world-creation settings.
- Proposal: schema 6 (chronology): `race`, `race_biome` (new: preferred biomes as rows), `culture`, `god` (creator gods), `age`, and the world-creation settings as required `meta` keys (including those P5 will use, so they need no later bump). Schema 7 (history engine): the event catalogue as tables `event_kind`, `event_kind_role` and `event_kind_value` (new; the world carries the kinds, roles, value keys and report templates it was generated with, so readers need no copy of the catalogue, and worldgen exposes the catalogue and the sentence renderer as library functions), `phase`, `event` (whose `kind` refers to `event_kind`), `event_participant`, `event_cause`, `event_value` (new), `civilization`, `settlement`, `settlement_census` (new), `region_control` (new), `relation` (with an end year). The later P3 stages add rows and catalogue entries, not columns.
- Interim choice: as proposed (P3-00 6.1); exact DDL in the P3-03 and P3-04 specs.
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated)

### Paul's decisions of 2026-11-19: the P3 interim choices are accepted; names are ASCII and unique

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history), 6 (world database), 13 (phase P3)
- Problem: the P3 overview (`docs/specs/p3-00-history-overview.md` section 15) and the first stage specs put fourteen questions to Paul, each with an interim choice.
- Proposal: none; a record. Paul accepted every interim choice: (1) quiet intervals advance in 25-year steps with tension checked after each; (2) year 0 is the first elven awakening and the present is the span; (3) P3 creates one named creator god per race, the rest of the pantheon is P5; (4) territory is held by whole regions; (5) events store facts and are rendered from templates, with no `summary` column, and `site_id` waits for P6; (6) event rules are code with their numbers in TOML in P3, data-driven rules are decided in P5; (7) roads and improved passes wait for P5; (8) 512 worlds run all races with fewer civilizations and are not judged for quality; (9) one culture per race in P3; (10) the history report is `report --history`; (11) the P5 world-creation settings are stored in `meta` from schema 6 and `--divine-script` is refused until P5; (12) the six cultures' first naming data stands as a starting point (a data edit later); (13) names are ASCII only (letters, space, apostrophe, hyphen); (14) names are unique across the whole world and across kinds.
- Interim choice: the P3 specs already follow these.
- Status: decided

### History: with a subset of races the first one present opens history; every race keeps its creator god

- Date: 2026-11-19
- Raised by: spec-writer (P3-03)
- Design doc section: 5 (Gods and the order of the races; awakening table; world-creation settings, "Races present: any subset; the order of appearance never changes")
- Problem: the design dates each awakening as a share of the span, with the elves at year 0, and allows any subset of races, but does not say when history starts if the elves are left out, or whether the god of a race that is left out exists.
- Proposal: the first present race awakens at year 0; every later present race keeps its own share of the span and its own small shift from the seed, so its date does not depend on which other races were chosen. Ages are numbered over the present races only. Every race has its creator god in the world, present or not, so a god keeps its name whichever races are chosen; a god whose race is absent created nothing in that world.
- Interim choice: as proposed (`docs/specs/p3-03-chronology.md` C3, 5.2, questions 1 and 2).
- Status: open

### History: active phases scale with the history span; the detail level sets the simulated share

- Date: 2026-11-19
- Raised by: spec-writer (P3-04)
- Design doc section: 5 (Deep time: "of which roughly 1,200 are simulated in detail"; world-creation settings: history span 2,000 to 12,000, history detail sparse, normal, dense)
- Problem: the design gives the simulated years for the default span only. Fixed phase lengths cannot keep the share of simulated years sensible across the span range: 1,200 years would be 60% of a 2,000-year history and 10% of a 12,000-year one. The design also does not say how the last race's opening phase and the final phase meet when the last age is short.
- Proposal: phase lengths are tuned in years at the 6,000-year default and scaled in proportion to the span, so the detail level fixes the share (sparse about 12%, normal about 19-20%, that is 1,110-1,200 of 6,000 before flare-ups, dense about 28%). The final phase is at most its length and starts where the last opening phase ends, so with the shipped numbers the orcs' age is usually active from their awakening to the present. A floor (10%) and a cap (45%, flare-ups included) are guaranteed by the scheduler. Each flare-up is marked by a `flare_up` event.
- Interim choice: as proposed (`docs/specs/p3-04-simulation-engine.md` sections 4.1 and 5, questions 1 to 3 and 6).
- Status: open

### History: peoples expand within reach of their capital and spread by secession

- Date: 2026-11-19
- Raised by: spec-writer (P3-05)
- Design doc section: 5 (Tick loop: "found a settlement, claim a region"; races: humans "spread quickly over land left open or ruined")
- Problem: the design does not say how far a civilization expands. Without a limit, in a peaceful history the first two races fill every region they like long before the later races awaken: in the first prototype the elves and goblins held 46 of 50 regions of a 1024 world and the humans awoke on a bare peak.
- Proposal: a civilization founds settlements only in free regions within a few hops of its capital region (elves, dwarves and faeries 1, goblins and orcs 2, humans 3, from the race's `expansion`), and only in land it rates at least 70% as well as its home. A succession crisis in a large realm may split it, and the new civilization expands from its own capital; that is how a people spreads beyond its reach. At the present about 31 of 49 regions are held at 1024, leaving free land for later races and for conflict.
- Interim choice: as proposed (`docs/specs/p3-05-peoples.md` 9.3, 10.3, questions 2 and 5).
- Status: open

### History: a population density per race

- Date: 2026-11-19
- Raised by: spec-writer (P3-05)
- Design doc section: 5 (races: elves "few in number", goblins "numerous"; Peoples table)
- Problem: the race data has no field that says how many of a people a region supports; lifespans and growth rates describe time, not numbers.
- Proposal: a new race field `density_permille` (elves 300, goblins 900, dwarves 700, faeries 250, humans 1000, orcs 800) scales the capacity of every region for that race. A data change, not a schema change.
- Interim choice: as proposed (`docs/specs/p3-05-peoples.md` 5.1, 12.2, question 1).
- Status: open

### History: random pressures act only in active phases; migration waits for conflict

- Date: 2026-11-19
- Raised by: spec-writer (P3-05)
- Design doc section: 5 (Quiet intervals: "populations drift toward what the land supports"; tick loop item 4; item 5 "migrations")
- Problem: the design lists plague, famine and succession crises as tick-loop pressures and migrations among the consequences, but does not say whether quiet intervals have pressures or what a migration is before there are wars.
- Proposal: famine, plague and succession crises act only in yearly ticks (active phases and flare-ups); quiet steps write growth, foundings and the fading of ruins only, which is the design's peace. Colonists and the survivors of an abandoned settlement move; migrations of whole peoples (refugees) come with conflict (P3-06) or P5.
- Interim choice: as proposed (`docs/specs/p3-05-peoples.md` 10.5, questions 4 and 6).
- Status: open
