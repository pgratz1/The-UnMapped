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
- Status: open. Merged into design.md, 2026-11-21.

### Paul's decisions of 2026-11-18: heath stripe, fen patches and habitability limits stay as they are

- Date: 2026-11-18
- Raised by: lead, decided by Paul
- Design doc section: 4 (geography: biomes, resources and habitability)
- Problem: two world-critic follow-ups were open. Biomes: heath shows as a stripe along mountain flanks on some worlds, and fens are blocky patches on the largest maps (`docs/reviews/p1-04b-biome-variety-critic.md`). Resources: the limits listed in the entry above (river banks below their hinterland, little contrast between lowlands, habitability falling to zero along the sharp line where mountain flanks begin).
- Proposal: none needed in the design doc.
- Interim choice: Paul decided to leave the heath stripe and the fens as they are, and to accept the habitability limits for now. Soil variation between lowlands is to be reconsidered only if the regions stage or the history phase needs more contrast. The explored alternatives (a height-above-valley term for heath, with or without noise) stay on branch `p1/04c-heath-fen-explore` and are not merged.
- Status: decided. Merged into design.md, 2026-11-21.

### Schema version 5: regions, region borders and biome counts per region

- Date: 2026-11-18
- Raised by: spec-writer, approved by the lead
- Design doc section: 4 (Regions), 6 (table list, schema versions)
- Problem: the design plans schema version 5 with tables `region` and `region_border`, without their columns. Beast placement and race preferences need the biome mix of a region, not only its dominant biome, and history needs to know where ore can be reached.
- Proposal: schema version 5, with the regions stage, adds `region` (id, name, dominant biome, bounds as four integer columns, seed cell, landmass, cell counts of lake, river, coast and walkable ground, largest river class, main walk area, mean and highest elevation, the means of the six resource scores, the 90th percentile of habitability, and counts of rich ore and of rich ore within 300 m of walkable ground), `region_border` (one row per pair of neighbouring regions: border length, the height and cell of the lowest crossing, walkable and large-river pair counts, largest river class, and the smallest id and number of natural passes crossing it) and `region_biome` (cells of each biome in each region; new, not in the design's table list).
- Interim choice: as proposed, specified in P1-06 and `docs/specs/schema.md`; generator 0.11.0.
- Status: open. Merged into design.md, 2026-11-21.

### Regions: how borders, large rivers, uplands and counts are settled

- Date: 2026-11-18
- Raised by: spec-writer, approved by the lead
- Design doc section: 4 (Regions)
- Problem: the design says regions are "bordered on ridges and large rivers" and "roughly 12, 45 and 180 per map", but not how borders are found, what a large river is, or what happens to mountain land. A prototype on generator 0.10.0 worlds showed that a climbing cost puts borders along contour lines, not ridges.
- Proposal: (1) borders come from a travel cost that is cheap along valleys and dear over ridges (cost weighted by height above the nearest stream); (2) a large river drains more than about 4 km², and its two banks belong to different regions where both have a settlement site; (3) add "Mountain massifs wider than about a kilometre form upland regions of their own; narrower ranges are divided along their crest between the regions on either side"; (4) counts: with 2 km² regions the measured counts are 13, 49 and 200, since land is about 40% of the map. Either say "about 2 km² each, roughly 13, 50 and 200 per map", or keep 12, 45 and 180 and accept regions of about 2.3 km². Paul's choice. (5) Placeholder names have the form "Northwest Wood II" (sector, biome noun, numeral), built from data; islands of any size are regions.
- Interim choice: as proposed, with 2 km² regions (seed spacing 1200 m, a tuning value), specified in P1-06. Paul decided on 2026-11-18 to keep 2 km² regions and to restate the counts in the design doc as roughly 13, 50 and 200 per map. Revision r2 of the spec, after the world critic's review (`docs/reviews/p1-06-regions-critic.md`), added two rules before the merge: a natural pass is attached to at most one region border, the one its path crosses nearest the col, so that a pass is never counted on two borders; and two more name nouns, "Mere" for a region that is at least half lake and "Fells" for an upland region whose dominant biome is not alpine, heath or highland moor. Merged as generator 0.11.0 on 2026-11-19.
- Status: open (decided by Paul; design doc section 4 to be updated). Merged into design.md, 2026-11-21.

### Viewer: layer units and region names come from a copy of the catalogue

- Date: 2026-11-18
- Raised by: viewer-engineer
- Design doc section: 6 (world database), 8 (viewer), 10 (`worldgen` public API)
- Problem: the viewer now shows each cell's value in display units (metres, degrees Celsius, millimetres a year, score out of 255) and the names of its biome and region. worldgen's public API names the palette of a layer but not its unit or meaning, so the viewer restates the layer catalogue of spec P1-00 section 4 (`gdext/src/viewer/layer_catalogue.cpp`); a unit change in the catalogue would not reach the viewer unless someone edits that copy. Likewise worldgen's `RegionRecord` (what `report` reads of table `region`) leaves out the name, so the viewer takes it from column 1 of the raw row after worldgen's `read_region_rows` has checked the rows. Smaller friction from the same work: in godot-cpp 10, `const godot::String + "literal"` is an ambiguous overload in some contexts and every such literal needs wrapping in `godot::String(...)`; and a legend is a hand-built `ItemList` with generated swatch textures, since Godot has no colour-legend control.
- Proposal: worldgen gains a public, read-only layer catalogue (name, unit, decimal scale, special values) beside `registered_palette`, and the region record carries `name`. The create-world screen and the in-game map will need the same information, so it belongs with the data rather than in each screen.
- Interim choice: the viewer keeps its copy of the units of P1-00 4 and reads the name column itself; a layer it does not know is shown under "Other" with its raw value.
- Status: accepted by Paul on 2026-11-22 (P4 overview decision 1). Merged into design.md, 2026-11-23.

### Paul's decision of 2026-11-19: P1 Geography approved; the critic's recommendations accepted

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 4 (geography), 5 (history), 13 (phases)
- Problem: phase P1 ends with Paul's review. The world critic's review of the finished geography (`docs/reviews/p1-06-regions-critic.md`) closes with recommendations for the history phase.
- Proposal: none; a record. Paul read the review, accepted its recommendations and approved P1. For P3 this means: the default map size, 1024, is the reference size for tuning and judging history, since 512 worlds have room for few peoples and suit a short history, not all six races; history locates a pass by its col (`pass.col_x`, `col_y`) and the `region` layer; a border counts as a river border by the share of its length that follows the river, not by any river pair; islets below about 0.3 km² are ignored as homes; homelands in wild uplands are chosen by region and biome, since little of that ground is walkable; ports come from history (forage and river mouths), since coasts score below the interior; lowland borders are political lines.
- Interim choice: the P3 specs follow these points.
- Status: decided. Merged into design.md, 2026-11-21.

### History: quiet intervals advance in steps so that flare-ups can interrupt them

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (Deep time: quiet intervals, flare-ups)
- Problem: a quiet interval "advances the world in one jump", yet it "can be interrupted" when a tension measure crosses a threshold. A single jump cannot be interrupted part-way, and the design does not say how often tension is checked.
- Proposal: a quiet interval is a sequence of quiet steps (25 years by default, tuning); after each step the tension measure is evaluated and, above the threshold, a flare-up (a short active phase) starts the next year. Consecutive quiet steps without change are written as one summary event spanning them.
- Interim choice: as proposed, in `docs/specs/p3-00-history-overview.md` 4.1 and 4.4.
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### History: the calendar starts at the first elven awakening

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (Deep time, awakening table), 6 (`meta` end year)
- Problem: the design gives years relative to the span ("year 0", "around year 2,100") but does not fix year 0 or the present, nor whether there are months.
- Proposal: years are integers; year 0 is the first elven awakening; the present is year E, the history span (6,000 by default), stored as `meta.history_end_year`; events within a year are ordered by event id. Eras shown to players are derived at runtime.
- Interim choice: as proposed (P3-00 2.1).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### History: P3 creates only the creator gods

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (Gods and the order of the races), 13 (P3 and P5 rows)
- Problem: each awakening is "an event with a god, a year and a place", but the pantheon and god interventions are phase P5.
- Proposal: P3 writes one named creator god per race (with its camp) into table `god`, so that awakening events name their creator; the rest of the pantheon, domains and interventions come with P5, which adds columns with a schema bump. P3 defines the seam for interventions and for the divine script (design 14): an intervention source consulted once per tick, empty in P3; `meta.param.divine_script` records the script's hash or nothing.
- Interim choice: as proposed (P3-00 9.2).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### History: territory is held by whole regions

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 5 (claim a region), 13 (P4 territory overlay by year)
- Problem: the design does not say at what grain territory is held, and P4's timeline must show it at any year cheaply.
- Proposal: a region is held by at most one civilization at a time; table `region_control` stores ownership intervals (region, from year, to year, civilization, causing event), and `settlement_census` the settlement states by year, so a viewer answers any year without replaying the log.
- Interim choice: as proposed (P3-00 6.4).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### History: the event log stores facts, not summaries

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 6 (table `event`: `summary`, `site_id`), 14 (enrichment needs structure)
- Problem: the design's `event` table has a `summary` column, while its rules say that text shown to players is generated at runtime from records and templates. Sites do not exist before P6.
- Proposal: `event` holds kind, year and end year, phase, region, settlement and a salience score; participants with roles, integer values and causes are in `event_participant`, `event_value` and `event_cause`. Sentences are rendered from a catalogue of event kinds with templates (data), in the history report, the viewer and the game. `site_id` is added with the sites in P6. The catalogue of kinds, roles and value keys is part of the schema contract, as the layer catalogue is.
- Interim choice: as proposed (P3-00 6.2, 6.3).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### History: event rules are code with TOML tuning in P3; roads and improved passes wait for P5

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 3 / 5 (event rules as hand-edited TOML; tick loop "build a road"; "Mountains, passes and roads in history"), 13 (P3 row)
- Problem: the design lists event rules among the TOML data and puts road building and pass improvement in the tick loop, while the P3 row of section 13 lists civilizations, settlements, wars and ruin only.
- Proposal: in P3 the rules are code from the stage specs, with every number in TOML tuning (kind `history`) and the report templates in data; whether conditions and effects should become data is decided in P5, once it is clear which rules need to vary. Roads and improved passes come with P5 (table `road`); P3 treats passes as fixed crossings.
- Interim choice: as proposed (P3-00 8.4, 15).
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### History: schema versions 6 and 7

- Date: 2026-11-19
- Raised by: spec-writer (P3-00)
- Design doc section: 6 (table list, schema versions)
- Problem: the design's table list is the target for all phases; P3 needs a subset, some tables the list does not have, and the `meta` keys for the world-creation settings.
- Proposal: schema 6 (chronology): `race`, `race_biome` (new: preferred biomes as rows), `culture`, `god` (creator gods), `age`, and the world-creation settings as required `meta` keys (including those P5 will use, so they need no later bump). Schema 7 (history engine): the event catalogue as tables `event_kind`, `event_kind_role` and `event_kind_value` (new; the world carries the kinds, roles, value keys and report templates it was generated with, so readers need no copy of the catalogue, and worldgen exposes the catalogue and the sentence renderer as library functions), `phase`, `event` (whose `kind` refers to `event_kind`), `event_participant`, `event_cause`, `event_value` (new), `civilization`, `settlement`, `settlement_census` (new), `region_control` (new), `relation` (with an end year). The later P3 stages add rows and catalogue entries, not columns.
- Interim choice: as proposed (P3-00 6.1); exact DDL in the P3-03 and P3-04 specs.
- Status: open (interim choice accepted by Paul, 2026-11-19; design doc to be updated). Merged into design.md, 2026-11-21.

### Paul's decisions of 2026-11-19: the P3 interim choices are accepted; names are ASCII and unique

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history), 6 (world database), 13 (phase P3)
- Problem: the P3 overview (`docs/specs/p3-00-history-overview.md` section 15) and the first stage specs put fourteen questions to Paul, each with an interim choice.
- Proposal: none; a record. Paul accepted every interim choice: (1) quiet intervals advance in 25-year steps with tension checked after each; (2) year 0 is the first elven awakening and the present is the span; (3) P3 creates one named creator god per race, the rest of the pantheon is P5; (4) territory is held by whole regions; (5) events store facts and are rendered from templates, with no `summary` column, and `site_id` waits for P6; (6) event rules are code with their numbers in TOML in P3, data-driven rules are decided in P5; (7) roads and improved passes wait for P5; (8) 512 worlds run all races with fewer civilizations and are not judged for quality; (9) one culture per race in P3; (10) the history report is `report --history`; (11) the P5 world-creation settings are stored in `meta` from schema 6 and `--divine-script` is refused until P5; (12) the six cultures' first naming data stands as a starting point (a data edit later); (13) names are ASCII only (letters, space, apostrophe, hyphen); (14) names are unique across the whole world and across kinds.
- Interim choice: the P3 specs already follow these.
- Status: decided. Merged into design.md, 2026-11-21.

### History: with a subset of races the first one present opens history; every race keeps its creator god

- Date: 2026-11-19
- Raised by: spec-writer (P3-03)
- Design doc section: 5 (Gods and the order of the races; awakening table; world-creation settings, "Races present: any subset; the order of appearance never changes")
- Problem: the design dates each awakening as a share of the span, with the elves at year 0, and allows any subset of races, but does not say when history starts if the elves are left out, or whether the god of a race that is left out exists.
- Proposal: the first present race awakens at year 0; every later present race keeps its own share of the span and its own small shift from the seed, so its date does not depend on which other races were chosen. Ages are numbered over the present races only. Every race has its creator god in the world, present or not, so a god keeps its name whichever races are chosen; a god whose race is absent created nothing in that world.
- Interim choice: as proposed (`docs/specs/p3-03-chronology.md` C3, 5.2, questions 1 and 2).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### History: active phases scale with the history span; the detail level sets the simulated share

- Date: 2026-11-19
- Raised by: spec-writer (P3-04)
- Design doc section: 5 (Deep time: "of which roughly 1,200 are simulated in detail"; world-creation settings: history span 2,000 to 12,000, history detail sparse, normal, dense)
- Problem: the design gives the simulated years for the default span only. Fixed phase lengths cannot keep the share of simulated years sensible across the span range: 1,200 years would be 60% of a 2,000-year history and 10% of a 12,000-year one. The design also does not say how the last race's opening phase and the final phase meet when the last age is short.
- Proposal: phase lengths are tuned in years at the 6,000-year default and scaled in proportion to the span, so the detail level fixes the share (sparse about 12%, normal about 19-20%, that is 1,110-1,200 of 6,000 before flare-ups, dense about 28%). The final phase is at most its length and starts where the last opening phase ends, so with the shipped numbers the orcs' age is usually active from their awakening to the present. A floor (10%) and a cap (45%, flare-ups included) are guaranteed by the scheduler. Each flare-up is marked by a `flare_up` event.
- Interim choice: as proposed (`docs/specs/p3-04-simulation-engine.md` sections 4.1 and 5, questions 1 to 3 and 6).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### History: peoples expand within reach of their capital and spread by secession

- Date: 2026-11-19
- Raised by: spec-writer (P3-05)
- Design doc section: 5 (Tick loop: "found a settlement, claim a region"; races: humans "spread quickly over land left open or ruined")
- Problem: the design does not say how far a civilization expands. Without a limit, in a peaceful history the first two races fill every region they like long before the later races awaken: in the first prototype the elves and goblins held 46 of 50 regions of a 1024 world and the humans awoke on a bare peak.
- Proposal: a civilization founds settlements only in free regions within a few hops of its capital region (elves, dwarves and faeries 1, goblins and orcs 2, humans 3, from the race's `expansion`), and only in land it rates at least 70% as well as its home. A succession crisis in a large realm may split it, and the new civilization expands from its own capital; that is how a people spreads beyond its reach. At the present about 31 of 49 regions are held at 1024, leaving free land for later races and for conflict.
- Interim choice: as proposed (`docs/specs/p3-05-peoples.md` 9.3, 10.3, questions 2 and 5).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### History: a population density per race

- Date: 2026-11-19
- Raised by: spec-writer (P3-05)
- Design doc section: 5 (races: elves "few in number", goblins "numerous"; Peoples table)
- Problem: the race data has no field that says how many of a people a region supports; lifespans and growth rates describe time, not numbers.
- Proposal: a new race field `density_permille` (elves 300, goblins 900, dwarves 700, faeries 250, humans 1000, orcs 800) scales the capacity of every region for that race. A data change, not a schema change.
- Interim choice: as proposed (`docs/specs/p3-05-peoples.md` 5.1, 12.2, question 1).
- Status: accepted by Paul, 2026-11-19 (deriving density from age appeals to him but would be hard to tune; the explicit field stays); design doc to be updated. Merged into design.md, 2026-11-21.

### History: random pressures act only in active phases; migration waits for conflict

- Date: 2026-11-19
- Raised by: spec-writer (P3-05)
- Design doc section: 5 (Quiet intervals: "populations drift toward what the land supports"; tick loop item 4; item 5 "migrations")
- Problem: the design lists plague, famine and succession crises as tick-loop pressures and migrations among the consequences, but does not say whether quiet intervals have pressures or what a migration is before there are wars.
- Proposal: famine, plague and succession crises act only in yearly ticks (active phases and flare-ups); quiet steps write growth, foundings and the fading of ruins only, which is the design's peace. Colonists and the survivors of an abandoned settlement move; migrations of whole peoples (refugees) come with conflict (P3-06) or P5.
- Interim choice: as proposed (`docs/specs/p3-05-peoples.md` 10.5, questions 4 and 6).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### History: wars are limited, fought for a claimed region, and end with the active phase

- Date: 2026-11-19
- Raised by: spec-writer (P3-06)
- Design doc section: 5 (tick loop "declare war, sue for peace", "battles, sieges, sackings"; flare-ups "two expanding borders meet"; quiet intervals)
- Problem: the design lists wars, sieges and sackings but does not say what a war is fought for, how it ends, or what happens to a war when an active phase gives way to a quiet interval, in which nothing is fought.
- Proposal: neighbours build strain from their creators' camps, old grudges, land hunger and fear of a giant; strain becomes a claim on one border region, the claim is the casus belli of a war, and the war is fought for that region: it ends when the attacker has it, when a side is exhausted, after 40 years at most, or at the first quiet step after its active phase, by battles won and settlements taken. Wars are declared only in yearly ticks; in quiet intervals strain keeps building and the highest strain is the tension that starts a flare-up, whose war follows without a draw. Relation kinds `enmity` (good against evil, from the younger people's founding), `war` (attacker first) and `peace`; alliances and vassalage wait for figures and factions in P5.
- Interim choice: as proposed (`docs/specs/p3-06-expansion-conflict.md` 4, 5, 7, questions 1, 2, 5).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### History: no race is wiped out; newly awakened peoples are sheltered

- Date: 2026-11-19
- Raised by: spec-writer (P3-06)
- Design doc section: 5 (Peoples table: every race has a role in play), 13 (P5 exit: every race takes part)
- Problem: with conquest, the late and small peoples (orcs, faeries, humans) were conquered to extinction in most prototype worlds; the design gives every race a role in play but does not say whether a race may vanish from history.
- Proposal: the last settlement of a race's last civilization never falls to a siege, and a civilization is not attacked in its first 150 years (it may attack). Civilizations still end by conquest; races do not.
- Interim choice: as proposed (`docs/specs/p3-06-expansion-conflict.md` 7.2, 7.3, question 4).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### History: a fighting strength per race; goblins less aggressive than orcs

- Date: 2026-11-19
- Raised by: spec-writer (P3-06)
- Design doc section: 5 (races: elves "few in number", goblins "numerous", orcs "strong, warlike"; elves' civilizations "the oldest in the present day")
- Problem: the race data has readiness for war (`aggression`) but nothing for how well a people fights; without it the few elves fall to the numerous goblins, and with the shipped aggression the goblins, numerous and split into several hordes, start four wars in five while the orcs, the design's warlike race, start one in seven.
- Proposal: a new race field `prowess_permille` (elves 1600, dwarves 1400, orcs 1300, faeries 1200, humans 1000, goblins 800) scales armies and garrisons; goblin `aggression` 700 → 600; aggression enters the strain rate squared. A data change, not a schema change.
- Interim choice: as proposed (`docs/specs/p3-06-expansion-conflict.md` 5.2, 6.1, 12.2, questions 3 and 6).
- Status: accepted by Paul, 2026-11-19 (interim choice stands); design doc to be updated. Merged into design.md, 2026-11-21.

### Paul's decisions of 2026-11-19 on the peoples questions: density stays a field; faeries live in forest, wet or not

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (races; Peoples table)
- Problem: `docs/specs/p3-05-peoples.md` section 19 put eight questions to Paul. He answered questions 1 and 7; the other six stand open with their interim choices.
- Decision:
  1. Question 1 (population density per race): the interim choice stands, a race field `density_permille`. Paul likes the idea of deriving density from a race's age but expects it to be hard to tune.
  2. Question 7 (faeries): faeries definitely live in forested land, and the forest does not have to be wet. The shipped weights at 0.15.0 favour wet land (`fen` 800, `wet_woodland` 800, `marsh` 600, `temperate_forest` 100), so this is a data change: forest biomes become the faeries' first preference whether wet or dry. It changes where faeries awaken and settle, so it changes world bytes and comes with a generator version bump.
- Interim choice: the 0.15.0 weights ship as validated; the faerie weights are re-tuned as a separate data change (spec amendment to P3-05 12.2 with a measured sweep) after P3-06 is integrated, before Paul's review of P3.
- Status: question 1 accepted; question 7 decided, data change pending. Paul confirmed the timing on 2026-11-19: the faerie change ships after P3-06, as its own data change. 2026-11-20: specified to ship with the P3-08 follow-up change (`docs/specs/p3-08-followups.md` item 4, P3-05 r6 12.3, generator 0.17.0). Merged into design.md, 2026-11-21.

### Races: sub-peoples with their own lands (dark elves underground, hill dwarves above ground)

- Date: 2026-11-19
- Raised by: Paul (recorded by the lead)
- Design doc section: 5 (races and cultures), 4 (geography: nothing underground yet)
- Problem: each race has one culture and one set of land preferences. Paul suggests that some elves could be dark elves living underground, and some dwarves hill dwarves living above ground in hillier country. The design doc has no notion of a branch of a race with different lands.
- Proposal: allow a race several cultures, each with its own biome and site preferences, settlement kinds and name syllables; a civilization takes one culture at its founding (a second civilization of a race, or one born by secession, may take another). Hill dwarves fit the present geography (upland and hill regions above ground). Dark elves need somewhere underground: geography has no caves or underground layer, so either a first approximation (mountain and pass regions, settlements flagged as underground) or a later geography addition. Both touch the data kinds `race` and `culture` (schema of the data files, and table `culture`), so they need a spec.
- Interim choice: none taken; P3 continues with one culture per race. Suggested home: P5 History depth, or an earlier small stage if Paul wants hill dwarves sooner.
- Status: decided for P5 by Paul on 2026-11-22 (no sub-peoples inside P5's stages); the later options stay an open question in design.md section 12 (2026-11-23)

### Paul's decision of 2026-11-19: the remaining peoples questions go with their recommendations

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (Peoples; tick loop; quiet intervals)
- Problem: after questions 1 and 7, six questions of `docs/specs/p3-05-peoples.md` section 19 were still open.
- Decision: Paul's words: "The rest in peoples.md can go with their recommended resolution." So:
  2. Reach from the capital: a people expands only a few hops from its seat and spreads farther by splitting. Accepted.
  3. `--civs-per-race` 1 to 4 is a request; a second civilization needs free land three hops from the first. Accepted.
  4. No famine, plague or crisis in quiet steps; they write growth, foundings and fading. Accepted.
  5. Secession as the outcome of some crises. Accepted.
  6. Migration of whole peoples deferred; refugee migration with conflict or P5. Accepted.
  8. A plague that empties a settlement does not spread to its neighbours. Accepted.
- Interim choice: none needed; the spec as implemented at 0.15.0 stands. Only question 7 (faeries in forest) leads to a change.
- Status: accepted; design doc sections 5 and 6 to be updated. Merged into design.md, 2026-11-21.

### History: magical artifacts are made in response to conflicts

- Date: 2026-11-19
- Raised by: Paul (recorded by the lead); Paul is not sure whether it is for now or later
- Design doc section: 5 (history: figures, casters, wars), 6 (derivation: sites and quest hooks)
- Problem: the design has no account of where magical artifacts come from. Paul wants them made in response to conflicts, so that each has a history within those conflicts and with the people who made it.
- Proposal: an artifact is an entity of history with its own table (maker civilization and settlement, the event that occasioned it, kind, name, and a chain of holders and places). It is created by an event whose cause is a conflict event (a war declared, a siege endured, a capital sacked, a long enmity), and it then moves through later events: carried to battle, captured in a sack, lost when a settlement is ruined, hidden, recovered. Where it lies at the present is a site and a quest hook for derivation. The conflict events it needs exist from P3-06 (wars, battles, sieges, sackings, conquests); makers want the figures and casters of P5.
- Interim choice: not in P3. Suggested home: P5 History depth, with figures and casters, built on P3-06's event log; a schema change (new tables and event kinds) with its own spec. If Paul wants a first version sooner, a small stage after P3-07 could make artifacts from wars alone, with civilizations as makers.
- Status: open for the design conversation; timing decided by Paul on 2026-11-19: artifacts wait for P5. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-19: region names get identity; no numbered names

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 4 (regions)
- Problem: regions are named by a sector word and a noun, with a Roman numeral when the base name repeats (spec P1-06 RG11). Paul: "East Wood" is fine but "West Wood III" is pretty boring; the names should come from a large set chosen at random so that regions have more identity, and should not rely on numbering.
- Decision: replace the numeral with names drawn from a large hand-written set held in data (TOML). A spec amendment to P1-06 RG11 defines the scheme and the word lists; it changes the `name` column of table `region`, so it changes world bytes and comes with a generator version bump. No schema change.
- Interim choice: the amendment is written now; it is implemented after P3-06 is integrated (together with the faerie data change or just before it), before Paul's review of P3.
- Status: decided; spec amendment written (P1-06 r3, branch `p1/06-region-names`); its questions answered below. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-19: the chronology questions go with their recommendations

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (races, ages, creator gods)
- Problem: `docs/specs/p3-03-chronology.md` section 16 put six questions to Paul.
- Decision: Paul's words: "In the chronology.md file I accept all the recommendations." So:
  1. With a subset of races that leaves out the elves, the first present race awakens at year 0 and each later race keeps its own share of the span.
  2. Every race keeps its creator god in table `god`, with the same name and id whichever races are chosen; a god whose race is absent created nothing in that world.
  3. Data errors across kinds are input errors (exit 3, before generating), not stage failures.
  4. Test-pattern worlds record the history settings they were given.
  5. The world-hash cases `hist-span-2000-512` and `hist-races-elf-orc-1024` came with chronology, `hist-dense-1024` with the engine.
  6. The world report gained `[chronology]` without a format bump (report v2).
- Interim choice: none needed; all six are as implemented at generator 0.13.0.
- Status: accepted; design doc section 5 to be updated for points 1 and 2. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-19: the simulation engine questions go with their recommendations

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (active phases, quiet intervals, flare-ups, the event log)
- Problem: `docs/specs/p3-04-simulation-engine.md` section 19 put six questions to Paul.
- Decision: Paul's words: "all the p3-04-simulation-engine.md recommendations are accepted." So:
  1. Phase lengths scale with the history span; the detail level fixes the share of simulated years (sparse about 12%, normal 19 to 20%, dense 27 to 29%).
  2. The last age is mostly active: the orcs' opening phase usually runs into the final phase, the design's "most recent age of conflict".
  3. The simulated share has a floor of 10% and a cap of 45%, as tuning, guaranteed by the scheduler.
  4. The event catalogue is code; templates and salience are data.
  5. Resume is a library seam, not a `generate --resume` option, until P5's history mode needs a command.
  6. A `flare_up` event marks each flare-up.
- Interim choice: none needed; all six are as implemented at generator 0.14.0.
- Status: accepted; design doc section 5 to be updated for points 1 to 3 and 6. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-19: the expansion and conflict questions go with their recommendations

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (wars, flare-ups, quiet intervals, race characters), 13 (P5 exit: every race takes part)
- Problem: `docs/specs/p3-06-expansion-conflict.md` section 19 put ten questions to Paul.
- Decision: Paul's words: "also accept all the ones in p3-06-expansion-conflict.md." So:
  1. Wars are limited: fought for one claimed region, ended when the attacker has it, when a side is exhausted, after 40 years at most, or at the first quiet step after their active phase.
  2. Wars happen only in yearly ticks; strain builds in quiet steps; a flare-up always brings its war.
  3. A fighting strength per race, `prowess_permille` (elves 1600, dwarves 1400, orcs 1300, faeries 1200, humans 1000, goblins 800), and goblin `aggression` 700 to 600.
  4. No race is wiped out: the last settlement of a race's last civilization never falls, and a civilization is not attacked in its first 150 years.
  5. Coalition by fear in place of alliances; no vassals in P3; alliances and vassalage come with figures and factions in P5.
  6. Aggression enters strain squared.
  7. Armies march across two borders.
  8. The event-count expectation of P3-00 6.1 is corrected to about 400 to 1,500 events at default settings and up to about 4,000 with the heavier settings.
  9. Humans on land left open or ruined is weak in P3 and accepted; refugee migration in P5 is the stronger reading.
  10. "A world already at war when the dwarves arrive" holds in about 30 of 50 worlds and is accepted; no rule is added to force it.
- Interim choice: none needed; all ten are as specified and implemented for generator 0.16.0 (in integration).
- Status: accepted; design doc sections 5 and 13 to be updated. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-19: the history report questions go with their recommendations

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 13 (P3: a text history report)
- Problem: `docs/specs/p3-07-history-report.md` section 19 put six questions to Paul.
- Decision: Paul's words, after accepting the conflict questions: "And on the history-report.md also." So:
  1. The summary budget (250 events), the minimum salience (20) and the milestone count (6) are constants of the report format, not tuning; what matters is steered by the per-kind base salience, which is data stored in every world.
  2. The summary level has at most 250 event lines, none below salience 20, the threshold rising with the size of the log.
  3. No filters in report format v1.
  4. A world without history is refused (exit 3), not given a partial report.
  5. The report uses the islet threshold 1172 as a constant; if that tuning value ever changes, an islet flag is stored in the world with that change.
  6. The report goes to standard output by default.
- Interim choice: none needed; all six are as specified; P3-07 is not yet implemented.
- Status: accepted. Merged into design.md, 2026-11-21.

### Paul's decisions of 2026-11-19 on the region name questions

- Date: 2026-11-19
- Raised by: lead, recording Paul's decision
- Design doc section: 4 (regions), 5 (how history sentences read)
- Problem: the region names amendment (`docs/specs/p1-06-regions.md` r3, section 14) put five questions to Paul.
- Decision:
  1. The largest region of each base name keeps the plain name ("East Wood"); the others draw a name.
  2. A leading "The" of a name is lower-cased in the middle of a sentence ("awakened the Elves in the Scarred Downs"); names are stored capitalised. This applies to any entity name a sentence renders, civilizations included, and not at the start of a sentence.
  3. Only famous real places and well-known fiction and game names are excluded; incidental coincidences with obscure real places are not a concern.
  4. The word lists are data Paul may edit (`game/data/placenames/`); no decision needed.
  5. Trait steering stays as shipped.
- Interim choice: none needed. Point 2 is a change to the sentence renderer (spec P3-04 8.4), specified and implemented together with the region names.
- Status: accepted; implementation scheduled after P3-06; 2026-11-20: specified for the P3-08 follow-up change (`docs/specs/p3-08-followups.md` items 1 and 2: P1-06 r4, P3-04 r10 8.5, P3-07 r7). Merged into design.md, 2026-11-21.

### Region names: who keeps the plain name, and "The" inside history sentences

- Date: 2026-11-19
- Raised by: spec-writer (P1-06 r3)
- Design doc section: 4 (regions: placeholder names), 5 and 14 (history text)
- Problem: with names drawn from hand-written lists (Paul's decision above), two choices are left. (1) Which regions keep the plain sector-and-noun name ("East Wood"): one per base name, or none. (2) Many drawn names begin with "The" ("The Scarred Downs"), stored capitalised as civilization names already are ("The Warrens of Bonk-Gitch"), so sentences read "Fielie awakened the Elves in The Scarred Downs."
- Proposal: (1) the largest region of each base name keeps it, every other region draws a name (45% plain at 1024, 16% at 2048). (2) Names are stored with the capital; the sentence renderer of the history report lower-cases a leading "The " of any entity name inside a sentence, for regions and civilizations alike.
- Interim choice: (1) as proposed (`docs/specs/p1-06-regions.md` r3, RG11 4); (2) stored names as proposed, sentences unchanged until the report spec takes the rule up (P1-06 r3 14.5 question 2).
- Status: accepted by Paul 2026-11-19 (both points as proposed): see "Paul's decisions of 2026-11-19 on the region name questions"; point 2 is specified in `docs/specs/p1-06-regions.md` r3 14.6. Merged into design.md, 2026-11-21.

### Names: generated names are filtered; gods have their own style; every realm name has a realm form

- Date: 2026-11-20
- Raised by: spec writer, after the world critic's review of P3-05 (`docs/reviews/p3-05-peoples-world-critic.md` F5)
- Design doc section: 5 (the seed varies the gods' names; civilizations and settlements are named), 14 (records a model can narrate)
- Problem: names are built from per-culture syllables with no check on what the result spells. In eight reviewed worlds the generator made a goblin settlement whose first syllables form a racial slur (and two near misses), human towns starting with a crude word for a body part, and gods with ordinary English names ("Bald", "Buzz", "Gord"). Gods use the same short shapes as villages ("Krok", "Sten"), and 7 of 14 elven realms had bare one-word names, so a realm reads like a town ("Finen founded Voeledell").
- Proposal: three rules, specified in `docs/specs/p3-02-names.md` r5 section 11. (1) **Names are filtered**: a drawn name is redrawn when the word that holds its root contains a listed fragment (anywhere, or at a word start), or is exactly a listed word; the lists are shared data for every culture (`game/data/name_filters/`: slurs, crude words, ordinary English words the syllables can produce, famous real and fictional names). (2) **Gods have their own style**: each culture names its gods from its own list of root shapes, shipped as the culture's shapes of two or more syllables, so no god is a one-syllable word. (3) **Every realm name has a realm form**: no culture names a civilization with a bare root (a loader check); the elven bare form becomes "The House of {root}".
- Interim choice: as proposed; ships in the follow-up change after P3-06 together with the region names and the faerie weights, with that change's generator version bump. No schema change. The region namer keeps its own word checks (P1-06 V4, V5), since the history lists' fragments reject ordinary English words such as "night" and "tithe".
- Status: open; spec amendment written (P3-02 r5, branch `p3/02-names-filter`), questions in P3-02 11.10; 2026-11-20: ships with the P3-08 follow-up change (P3-02 r6: every word of the rendered name covered, the redraw stated; generator 0.17.0); the questions stay open. Merged into design.md, 2026-11-21.

### History: dwarves settle high ground; abandonment names where the survivors went; one quiet era per quiet stretch

- Date: 2026-11-20
- Raised by: spec writer (P3-08), after the world critics' reviews of P3-05 and P3-06 (`docs/reviews/p3-05-peoples-world-critic.md` F1–F4, `docs/reviews/p3-06-conflict-world-critic.md` F8, section 5)
- Design doc section: 5 (dwarves "in the uplands", faeries "hidden communities in wild places", quiet intervals "four centuries of peace under the same dynasty", abandonment and ruins), 6 (text rendered from records and templates), 14 (records a model can narrate)
- Problem: (1) every race settles the lowest farmland of its region, so dwarves build lowland forest cities; (2) an abandoned settlement's survivors and the blow that began its decline are not recorded, so the log reads as a people walking away after a small blow; (3) a quiet era closes at every event of its civilization, so the log fills with 25-year eras and "(0 generations)"; (4) victors of a lopsided battle lose more than the defeated; (5) a capture sentence ends "and 0 fled"; (6) small plagues and crises without outcome crowd the summary; (7) a people's last village can be abandoned by a coin toss.
- Proposal: specified in `docs/specs/p3-08-followups.md` (generator 0.17.0, no schema change): a per-race cell term in the site choice (dwarves: ore and height) and dwarven holds off the walkable ground are fortresses (P3-05 r6 6.2, 8.4); a new event kind `settlement_abandoned_to` naming the destination, and the decline's first blow as a second cause (P3-05 r6 8.5); one quiet era per civilization and quiet stretch (P3-04 r10 6.6); victors' losses from the smaller force (P3-06 r7 7.3); a three-word template form that drops a clause whose number is 0 (P3-04 r10 8.4); saliences by toll and outcome (P3-05 r6 10.2, 10.3); no chance abandonment of a last settlement (P3-05 r6 8.5). Plus the volume change of P3-08 6.4.
- Interim choice: as proposed, for the P3-08 implementation; the questions to Paul are in `docs/specs/p3-08-followups.md` 10.
- Status: open. Merged into design.md, 2026-11-21.

### History: four rule changes proposed by the world critic's review of conflict

- Date: 2026-11-20
- Raised by: lead, from the world critic's review of P3-06 (`docs/reviews/p3-06-conflict-world-critic.md`)
- Design doc section: 5 (history: wars, ruin, the elder peoples), 13 (P3)
- Problem: conflict (generator 0.16.0) gives the history causes, endings and ruins, and the review found nothing that blocks its merge. It found four things the design does not settle:
  1. **The last refuge cannot fall.** A people's last settlement is protected so that no race goes extinct. The guard is in use in 92 of 100 default worlds; one village holds 32 sieges in 352 years. It reads as absurd.
  2. **Only goblins act.** Goblins declare 81% of wars at defaults; elves and faeries declare none, however much they have lost, and orcs lose three times as often as they win.
  3. **Many wars change nothing.** 42% of default wars end with no sack, capture, cession or conquest, and the same pair fights over the same region up to 17 times.
  4. **A race can win though no civilization does.** The runaway guard measures land share per civilization only; in 38 of 100 default worlds one race has at least 60% of all people.
- Proposal:
  1. A last refuge falls: its survivors flee and found a hidden hamlet elsewhere. The guard stays only when there is nowhere to flee.
  2. Land lost by capture, sack or cession leaves a revenge claim that acts whatever the race's aggression, so elves retake ruins and dwarves avenge their holds. Orcs are made able to win their age (a larger levy, or a limit on declaring against much stronger foes).
  3. A won war always costs the loser: land, tribute, or a longer truce for a pair that keeps fighting over the same goal.
  4. Runaway is also measured per race, in the spec's sweep statistics and the validation sweep.
- Interim choice: P3-06 merges as specified. Points 1 to 4 are being specified as their own change, P3-09 "conflict balance" (generator 0.18.0, branch `p3/09-spec`), at the spec writer's most reasonable reading, after the already decided names and peoples follow-ups (P3-08, generator 0.17.0). The P3-09 spec will list the decisions for Paul to confirm before it is implemented.
- Status: accepted by Paul on 2026-11-20 (see "Paul's decision of 2026-11-20: the P3-09 conflict balance questions"). Merged into design.md, 2026-11-21.

### Paul's decisions of 2026-11-20 on the name filter questions

- Date: 2026-11-20
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (names of gods, peoples and places)
- Problem: the names filter amendment (`docs/specs/p3-02-names.md` section 11.10) put seven questions to Paul.
- Decision: Paul's words: "on 1, the short reject list is correct, the happy accidents should be preserved. I accept the rest of your recommendations on the names." So:
  1. The English-word list is the short one: only words that are crude, comic or modern-sounding are rejected (about 150). Ordinary English words that fit the setting stay as name roots ("The Kingdom of Gold", "Thorn Deep", "Moss Hollow"). The slur and crude-fragment lists are separate and unchanged.
  2. Region names do not share the filter; they keep their own exclusion list.
  3. No edits to the syllable lists; the filter does the work.
  4. Gods use each culture's `god_patterns` (its multi-syllable shapes). Epithets wait for the pantheon in P5.
  5. The elven bare realm form is "The House of {root}".
  6. Orcish two-letter apostrophe parts ("Am'gash") stay.
  7. The filter lists are plain text in the repository.
- Interim choice: points 2 to 7 are as shipped with P3-08 (generator 0.17.0). Point 1 differs from what P3-08 ships (the full list of 647 words): the short list is a data change to `game/data/name_filters/words.toml` that changes worlds, and ships with the next change after P3-08.
- Status: accepted; point 1 scheduled after P3-08; 2026-11-20: point 1 specified (`docs/specs/p3-02-names.md` r8, section 11.11, branch `p3/10-word-list`): `words.toml` holds 393 of the 647 words (with `mold` restored by the lead's interim choice) (crude, comic or modern-sounding words, and function words, inflected forms and plain slips), 254 are released; a data change only (no code, no schema change), shipping with P3-09 (0.18.0) or alone as 0.17.1. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-20: the P3-08 tuning questions go with their recommendations

- Date: 2026-11-20
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history: settlements, famine and plague, war), 13 (P3)
- Problem: the P3-08 change spec (`docs/specs/p3-08-followups.md` section 10, branch `p3/08-spec`) put five tuning questions to Paul.
- Decision: Paul's words: "I accept all your recommendations in P3-08 tuning." So:
  1. Dwarven sites: dwarves take the high, ore-rich part of their regions (cell weights ore 400, height 600); most dwarven foundings are fortresses.
  2. Battle losses are computed from the smaller force, so a victor never loses more than the defeated, accepting that more sieges succeed.
  3. One quiet era per civilization per quiet stretch.
  4. Famine and plague-spread rates are halved.
  5. The sentence templates gain the three-word form (a wording chosen by a value).
- Interim choice: none needed; all five are as implemented in P3-08 (generator 0.17.0), which is in validation. The validation sweep and the world critic's review check whether the darker battles or the dwarven fortresses overshoot.
- Status: accepted. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-20: the P3-09 conflict balance questions go with the lead's recommendations

- Date: 2026-11-20
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history: wars, ruin, the elder peoples), 13 (P3)
- Problem: the P3-09 change spec (`docs/specs/p3-09-conflict-balance.md` section 8, branch `p3/09-spec`) put eleven questions to Paul, four of them design-level (the entry "four rule changes proposed by the world critic's review of conflict").
- Decision: Paul's words: "I accept all your recommendations on P3-09 conflict balance." The lead's recommendations were the spec's, so:
  1. A race's last refuge falls into flight: it is sacked and its survivors found a settlement in hiding up to three borders away, out of every army's sight for 300 years, found again without an event. The old guard holds only when no free land is in reach.
  2. A lost war always costs the loser, also when the defender wins: the goal, else the loser's best non-capital region in the winner's reach, else a tribute of 100 permille of its people, carried off. Tribute as people is taken now and does not wait for wealth in P5.
  3. Revenge claims: a people that lost a settlement or region fights the taker at aggression at least 400 while the grudge lasts.
  4. Runaway is also measured per race, as statistics with typical ranges, not a hard bound; the share is watched in the validation sweep.
  5. Orcs starting about 25% of the wars of their own age is accepted, since they now win more than they lose.
  6. Faeries never strike back.
  7. The race fields `muster_permille`, `conceal_min` and `home_defense_permille`, with the shipped values.
  8. Local levies, odds weighed by terrain, an odds minimum that also stops flare-ups, and a shorter truce for the most warlike.
  9. Armies cross three borders against an enemy of the other camp.
  10. No conflict rule for map size; the automatic setting founds more civilizations per race on a larger map.
  11. The history report's summary: a per-age floor of 20, a shown declaration always with its ending, claims at salience 35, the notes; within the 250-line budget and minimum salience 20. The fill step is added only if the P3-07 implementer's measurements confirm it.
- Interim choice: none needed. The spec's figures were measured on a prototype at 0.16.0 and are re-measured after implementation on top of P3-08.
- Status: accepted; implementation follows P3-08. Merged into design.md, 2026-11-21.

### History: conflict balance beyond the four rule changes (P3-09)

- Date: 2026-11-20
- Raised by: spec writer (P3-09), after the world critic's review of P3-06 (`docs/reviews/p3-06-conflict-world-critic.md` F1–F7, F9)
- Design doc section: 5 (faeries "hidden communities in wild places"; orcs "strong, warlike", "their coming opens the most recent age of conflict"; goblins "hostile to elves from the first year"; "old wars leave grudges"), 2 (map size means more land), 13 (P3 exit: the history reads as coherent history)
- Problem: besides the four design-level changes of the lead's entry (flight of the last refuge, revenge, a cost for every lost war, runaway measured per race), the review's findings need rules the design does not state: faeries are besieged dozens of times in open view; orcs, the most warlike people, lose three wars in four in their own age; the same defender is attacked over the same wood up to 17 times; the goblin–elf feud is missing in a fifth of worlds; a 2048 world is nearly at peace; the summary drops whole ages and war endings.
- Proposal: specified in `docs/specs/p3-09-conflict-balance.md` (generator 0.18.0, no schema change; normative text in P3-06 20, P3-05 21, P3-01 3.2, P3-07 21): faerie settlements in their forest are concealed from armies, elves and faeries defend their own land more strongly (race fields `conceal_min`, `home_defense_permille`); orcs muster four times the standard levy (race field `muster_permille`), battles are fought with the levies near the battlefield, a people declares only wars whose odds, weighed by the terrain, reach a minimum, and peoples more warlike than goblins keep shorter truces; a defender that has withstood three wars over one region is not attacked over it again until it changes hands; good and evil armies cross three borders to reach each other; a larger map founds more realms per race; the history report's summary shows at least 20 events per age and every shown war with its ending.
- Interim choice: as proposed, for the P3-09 implementation; the questions to Paul were `docs/specs/p3-09-conflict-balance.md` 8 (items 5–11).
- Status: accepted by Paul on 2026-11-20 (see "Paul's decision of 2026-11-20: the P3-09 conflict balance questions go with the lead's recommendations"). Merged into design.md, 2026-11-21.

### Paul's notes of 2026-11-20 from reading a generated history (seed 42, generator 0.17.0)

- Date: 2026-11-20
- Raised by: Paul, recorded by the lead
- Design doc section: 5 (history: wars, the peoples, relations between civilizations), 13 (P3, P5)
- Problem: Paul read the summary history of seed 42 at map size 1024 and noted three things.
  1. **A defender sues for peace right after winning the last battle** (the dwarves in 3826, the elves in 4799 and 5625). Paul: "that maybe doesn't make sense?" The cause: a war ends when a side's dead pass a share of its people (exhaustion), the loser is decided by the count of battles won, and neither looks at who just won or by how much. In 3826 the Hold of Dulimin killed 991 goblins for 44 of its own, and those 44 took it over the threshold, so it "sued for peace".
  2. **Faeries are far too strong in battle**: they lose a handful where goblins lose thousands (4424: 881 goblins and 4 faeries fell). Paul: "maybe instead of getting involved in lots of battles the Faeries could be so hard to find that they often can't be conquered?" The cause: the defeated lose a share of their own force whatever the size of the victor's, so forty faeries can kill nine hundred.
  3. **Alliances and trade.** Paul: "we should probably add some kind of alliance and trade systems. I could imagine different races that are closely aligned forming trading partnerships that could enrich and strengthen both. Also during battles, one could imagine a close partner might come to the aid of an ally."
- Proposal:
  1. A side that has just won a battle does not sue for peace in that step, and the scale of battles counts in deciding who won a war, not only their number. Wording: "sued for peace" is used only where the loser yields.
  2. The dead of the defeated are bounded by what the victors' force can kill. Faeries avoid battle: an army entering their forest often finds no one (no battle, no siege), instead of being beaten by a stronger defence.
  3. Alliances and trade as a relation between civilizations: aligned peoples in contact form partnerships that add to both sides' growth and strength, and an ally may join a partner's war or battle.
- Interim choice: points 1 and 2 are added to the P3-09 "conflict balance" change (generator 0.18.0), which is being implemented, as an amendment to its spec; with P3-09's rule that a lost war always costs the loser, point 1 would otherwise make a victorious defender cede land. Point 3 is a new system (new relation kinds, a notion of wealth, allied armies) and is proposed for P5 History depth, with the artifacts made in response to conflicts; it is not specified in P3 unless Paul wants it earlier.
- Status: 1 and 2 accepted for P3-09 (Paul's notes are the decision; the rule details are the spec writer's and are measured); 3 decided by Paul on 2026-11-20: "P5 is where the alliances and trade and wealth should go." It joins "race relations and mixed settlements" in P5 History depth, with the artifacts made in response to conflicts. Merged into design.md, 2026-11-21.

### History: two ideas from the world critic's review of P3-08

- Date: 2026-11-20
- Raised by: lead, from the world critic's review of P3-08 (`docs/reviews/p3-08-followups-world-critic.md`)
- Design doc section: 4 (regions), 5 (history: the peoples)
- Problem: the review found P3-08 acceptable with follow-ups, nothing blocking. Most follow-ups are wording and data and are scheduled as a change after P3-09. Two are design-level:
  1. **Region names are drawn before history and never change.** Grim names are dense (15 of 29 drawn names in one world), so the elves' untroubled homeland of 6,000 years can be "Dreadlaw Hurst", while the largest and most fought-over regions keep plain compass names ("North Downs").
  2. **Upland dwarves have fallen silent.** On high, ore-rich ground they are out of everyone's reach: one dwarven realm has 17 events in 2,700 years, no wars and two lines in the summary. History only happens to mountain peoples when someone brings a war to them.
- Proposal:
  1. History renames regions: a region's defining event (a great sack, a long war, a founding, a plague) can give it its present-day name, with the old name kept in the record.
  2. Mountain peoples get history of their own: delvings, lost holds and mines as events, not only wars.
- Interim choice: neither is specified. Both suit P5 History depth (the first also needs the record of renamed places that sites and quests in P6 would use). The grim share of drawn region names can be lowered in data before then.
- Status: accepted by Paul on 2026-11-22 for P5 History depth (see "Paul's decision of 2026-11-22: regions renamed by history, and a history for the mountain peoples"); both listed as open questions in design.md section 12 (2026-11-21). Merged into design.md, 2026-11-23.

### Region names: the plain compass name, a second look

- Date: 2026-11-20
- Raised by: spec writer (P3-10), from the world critic's review of P3-08 (`docs/reviews/p3-08-followups-world-critic.md` F7)
- Design doc section: 4 (regions: placeholder names until history gives cultural names)
- Problem: Paul decided on 2026-11-19 that the largest region of each base name keeps the plain sector-and-noun name ("East Wood"). The largest region is often the most fought over, so the busiest regions carry the plainest names (seed 42 at 1024 keeps seven compass Woods, three of them side by side where the main feud is fought), and small maps stay mostly plain: 83% of regions at 512, 42% at 1024, 15% at 2048 (the P3-08 validation sweep's region rows).
- Proposal: keep one plain name per kind of land: the largest region of each land noun keeps its compass name (one "… Wood", one "… Downs" per world), every other region draws a name. Measured on the same rows: 33% plain at 512, 12% at 1024, 4% at 2048. Alternatives measured: plain names only for a base name that occurs once (68%, 18%, 4%); the middle-sized region of each base name keeps it instead of the largest (shares unchanged).
- Interim choice: unchanged until Paul answered; `docs/specs/p1-06-regions.md` r6 15.5 had the measurements.
- Status: accepted by Paul 2026-11-20 (see "Paul's decision of 2026-11-20: the P3-10 questions go with their recommendations"); normative in `docs/specs/p1-06-regions.md` r7 15.5. Merged into design.md, 2026-11-21.

### History: one quiet line for the world

- Date: 2026-11-20
- Raised by: lead and spec writer (P3-10), from the world critic's review of P3-08 (F3)
- Design doc section: 5 (quiet intervals: "four centuries of peace under the same dynasty"), 6 (text rendered from records)
- Problem: since P3-08 each civilization writes one `quiet_era` per quiet stretch, but every civilization writes its own line for the same stretch with the same years. In 40 default worlds at generator 0.18.0 every quiet row's eras share their years: 1,640 lines in 325 rows; with four civilizations per race a row holds up to 12 identical lines (0.17.0).
- Proposal: one world-level event per quiet stretch (`quiet_stretch`, P3-04 r12 20.1), with every living realm as a participant and their number as a value ("For 425 years, from 3392 to 3816, the land was quiet, and 5 realms kept the peace."). The per-realm count of ruling generations is derivable from the stored rows and is printed by the history report in each realm's milestones. Alternative: keep per-realm eras and hide those shorter than about 100 years in the report (25% of the lines at 0.18.0).
- Interim choice: the world-level line, in the P3-10 change (generator 0.19.0; `docs/specs/p3-10-followups.md` question 2).
- Status: accepted by Paul 2026-11-20 (see "Paul's decision of 2026-11-20: the P3-10 questions go with their recommendations"). Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-20: the P3-10 questions go with their recommendations

- Date: 2026-11-20
- Raised by: lead, recording Paul's decision
- Design doc section: 4 (regions), 5 (history: how it reads, where peoples settle)
- Problem: the P3-10 change spec (`docs/specs/p3-10-followups.md`, branch `p3/10-spec`: the follow-ups of the world critic's review of P3-08) put questions to Paul; the lead presented five with recommendations.
- Decision: Paul's words: "Yes I agree with the recommendations on all five." So:
  1. **Plain compass names.** One plain name per land type per world, kept by that land type's largest region ("East Wood"); every other region draws a name. This replaces the earlier rule that the largest region of each base name keeps it (decision of 2026-11-19, region name question 1). Measured by the spec writer: plain names fall from 83% / 42% / 15% of regions at map sizes 512 / 1024 / 2048 to 33% / 12% / 4%.
  2. **Quiet eras.** One world-level line per quiet stretch, with the realms that passed it as participants, instead of a line per realm.
  3. **Thousands separators** in the numbers of rendered sentences from 1,000 on, not in years.
  4. **Faerie and dwarven sites** as specified: faeries drawn inland and away from the coast, dwarves without a harbour bonus and with habitability in the site value. The lead watches the measured side effect (humans lost 15% of standing settlements in 20 worlds) in validation.
  5. **Two comic names stay** unfiltered ("Mammga", "The Twiwhiwou Ring"): each comes from an ordinary syllable of its culture.
  The lead also accepts the spec's technical question (a template placeholder for the cause clause of abandonment and dwindling sentences).
- Interim choice: none needed. P3-10 (generator 0.19.0) is implemented after P3-09.
- Status: accepted. Merged into design.md, 2026-11-21.

### History: six questions from the world critic's review of P3-09 (conflict balance)

- Date: 2026-11-20
- Raised by: lead, from the world critic's review (`docs/reviews/p3-09-conflict-balance-world-critic.md`, added with the P3-09 merge)
- Design doc section: 5 (history: wars and how they end, the races' roles)
- Problem: the review passes P3-09 with notes; nothing blocks the merge. War endings now read right, flights read well and no race goes extinct. Six things remain, each a rule or tuning choice:
  1. **A war is "won" when a third party takes the goal.** The attacker is given the win, and a tribute or cession, even when it lost the fighting (42 such endings in 100 worlds; in 19 the attacker took nothing, in 7 it also lost more battles than it won).
  2. **Faerie searches repeat.** A failed search for the faeries is faerie-like once and an anticlimax by the fourth (median 4 per world, up to 11); every one is in the summary; in 67 of 437 the searcher had already fought in that wood and still "found no one".
  3. **One wood fought over eleven times.** A sack empties a region, its owner resettles it, and that counts as a change of owner, which resets war-weariness.
  4. **Orcs are a threat only in aggregate.** They win more than they lose overall, but declare no war in 24 of 100 worlds and win none in 44.
  5. **Goblins start most wars** (76% overall; at least 90% in 24 of 100 worlds), and one race holds 700‰ or more of the people in 27 of 100 worlds (goblins in 21).
  6. **A refuge that "fled into hiding" founds colonies and declares wars while hidden** (7 of 46 flights).
- Proposal (the lead's recommendations):
  1. The attacker wins by aim only if it holds the goal itself; otherwise the war is decided by score with the last-battle rule, falling back to a truce.
  2. One failed search makes that wood weary for every claimant for a time; salience of the search drops below the default summary threshold except for the first in a world; a second sentence for wars that had already fought there.
  3. Resettlement by the same owner does not reset weariness.
  4. Orcs declare at shorter odds than other races (a per-race `odds_min`).
  5. Take 1 to 4 first and measure again; if goblins still lead nine wars in ten in a quarter of worlds, raise human aggression.
  6. A hidden refuge lies low: no colonies and no declarations for its hidden years.
- Interim choice: P3-09 merges as validated. These six go into the next change after Paul's answer (with P3-10, generator 0.19.0, or a change of their own).
- Status: accepted for 1 to 4 and 6; 5 answered (no change). See the decision entry below. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-20 on the six questions from the P3-09 review

- Date: 2026-11-20
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history: wars and how they end, the races' roles)
- Problem: the entry "History: six questions from the world critic's review of P3-09 (conflict balance)" above.
- Decision: Paul's words: "let's take your recommendation for 1-4 and 6. On 5 it's ok for goblins to start most wars, do not increase human aggression". So:
  1. An attacker wins by aim only if it holds the goal itself; otherwise the war is decided by score with the last-battle rule, falling back to a truce.
  2. One failed search makes that wood weary for every claimant for a time; only the first failed search in a world reaches the default summary; a second sentence for wars that had already fought there.
  3. Resettlement of a region by the same owner does not reset weariness.
  4. Orcs declare war at shorter odds than other races (a per-race `odds_min`).
  5. Goblins starting most wars is accepted. Human aggression is not raised.
  6. A hidden refuge lies low: no colonies and no declarations for its hidden years.
- Interim choice: none needed. The five rule changes are added to the P3-10 change (generator 0.19.0), which follows the P3-09 merge.
- Status: accepted. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-20: orcs awaken with a quarrel

- Date: 2026-11-20
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history: the races' roles, the orc age)
- Problem: the per-race odds accepted for orcs (decision above, item 4) cannot reach worlds where orcs never raise a claim: in 5 of the 9 worlds (of 40) where orcs declare no war, they have no quarrel with anyone, so about 80% of worlds would have an orc war against a target of 90%.
- Decision: Paul's words: "having the orcs start with a quarrel with a neighbor is reasonable". Orcs begin their history with a quarrel with a neighbour, in addition to the shorter odds. The rule is specified in the P3-10 change (generator 0.19.0), not held back until the target is measured.
- Interim choice: none needed.
- Status: accepted. Merged into design.md, 2026-11-21.

### Viewer: a history movie on the 2D map

- Date: 2026-11-21
- Raised by: lead, recording Paul's request
- Design doc section: 8 (viewer), 13 (phase P4 Viewer v1)
- Problem: Paul's words: "it would be nice to be able to visualize the rise and falls of civilizations using the godot 2d map, as a movie. Doesn't have to be soon, but something to put on the list". The viewer shows geography only; nothing shows how the realms grow, fight and fall over the years.
- Proposal: a time control on the 2D map of the viewer: a year slider with play, pause and speed, which colours each region by the civilization that holds it in that year and marks the standing settlements, with battles, sackings and falls shown as they happen and the year's sentences beside the map. The data is already in the world file: `region_control` stores ownership intervals and `settlement_census` the settlement states by year, so any year is answered without replaying the log. Saving the playback as a video file is a second step, after the playback works on screen. Candidate for P4 Viewer v1.
- Interim choice: none needed; not scheduled. To be placed when P4 is planned, after Paul's review of P3.
- Status: open; listed in design.md as a candidate for phase P4 and as an open question in section 12 (2026-11-21). Merged into design.md, 2026-11-23.

### Paul's decision of 2026-11-21: orcs at peace among goblins is accepted

- Date: 2026-11-21
- Raised by: lead, recording Paul's decision
- Design doc section: 5 (history: the races' roles, the orc age)
- Problem: with the awakening quarrel and the shorter odds, about 80% of default worlds have an orc war, against the 90% first aimed at. The rest are mostly worlds where only goblins, the orcs' own camp, are within reach.
- Decision: Paul's words: "Orcs surrounded by goblins and not fighting is fine on 20%of worlds, better than reopening and lots of changes". The share of worlds with an orc declaration is a reported statistic, not a merge condition. Orcs do not fight their own camp and do not seek distant enemies. Asked whether a longer orc age (about 1000 more years) should be tried, Paul decided on the same day: "leave it as is for now". After the second round of P3-10 the share is 71 of 100 default worlds, because an orc realm with one settlement holds back its quarrel; Paul on 2026-11-21: "71% is fine".
- Interim choice: none needed.
- Status: accepted. Merged into design.md, 2026-11-21.

### Paul's decision of 2026-11-21: faerie names keep their character, without the baby-talk sounds

- Date: 2026-11-21
- Raised by: lead, recording Paul's decision (question 2 of the world critic's review of P3-10)
- Design doc section: 5 (races and cultures: names)
- Problem: faerie names are built from soft consonants and bright vowels. At their best they read as light and fey (Lyfae Hollow, The Faeli Court, Atwia Glade); at their worst as baby talk (Wifleeflee Glade, Weebaema Glade, Eepou Glade, The Twiwhiwou Ring). The name filter removes crude names, not these. The alternatives were to leave the syllables alone, or to make them strange in a way that drifts toward the elven names.
- Decision: the middle option, which Paul accepted ("Agree the middle option is good"): keep the faerie character, and drop the sounds that cause the baby talk: no `p` and `b` onsets, fewer `ee` and `ou` nuclei, and no syllable repeated twice in a row. A change to the faerie culture's naming data (`game/data/cultures/faerie.toml`) and, for the repeated syllable, a naming rule if the data cannot express it.
- Interim choice: not part of P3-10 (generator 0.19.0), which is in validation. It goes into the next change that alters world bytes, with a spec. It is specified in P3-11 (generator 0.20.0; spec P3-02 section 14), which also removes the coda `p` (names ending in "-p"), since it sounds like the onsets ruled out. Paul, 2026-11-21: "Dropping the -p ending is fine".
- Status: accepted. Merged into design.md, 2026-11-21.

### History: the P3-10 orc and aim rulings (lead, awaiting Paul)

- Date: 2026-11-21
- Raised by: lead (rulings R1 and R2), written up by the spec writer
- Design doc section: 5 (history: the races' roles, the orc age, wars and how they end)
- Problem: the world critic failed P3-10 (generator 0.19.0) on two rules Paul decided on 2026-11-20 (`docs/reviews/p3-10-followups-world-critic.md` F1, F2).
  - F1, the orcs. The awakening quarrel and the shorter orc odds ("orcs awaken with a quarrel"; decision 4 of the P3-09 review) were meant to make the orcs warlike, and made them weaker. A new orc realm declares on its quarrel neighbour with one settlement, loses, flees, and its refuge lies hidden for 300 years, longer than what is left of the orc age. At 1024 defaults the orcs end with one standing settlement in 21 of 100 worlds (7 at 0.18.0) and do nothing after 5750 in 9 (0). In `calm` and `turbulent` they lose more wars than they win.
  - F2, aim. Decision 1 of the P3-09 review ("an attacker wins by aim only if it holds the goal itself") lets a war over a region the attacker already holds be won at its first check, with no battle, wherever the defender keeps one town there. In seed 23 the Urarz Horde takes tribute three times from the Kingdom of Durd, which keeps the town Fiecaes in the Horde's Ancient Holt. This happens in 22 wars in 100 default worlds. It is a gap in the rules, not a defect of the code (`docs/specs/p3-06-expansion-conflict.md` 22.5).
- Proposal (the lead's rulings, specified in P3-06 r17 section 22 and `docs/specs/p3-10-followups.md` section 12):
  - R1 (a): a realm with fewer than two standing settlements does not declare war on its awakening-quarrel claim. The claim stays open and is pursued once the realm has two. Only the quarrel claim is held back.
  - R1 (b): the years a fled refuge stays hidden become a race field: orcs 100, every other race 300 as now. A broken orc realm can then act again inside its own age.
  - R1, targets at 1024 defaults (100 worlds): orcs with one standing settlement at the present in at most 10 worlds; orcs doing nothing after 5750 in at most 2; orc wins at least orc losses; the same reported for `calm` and `turbulent`.
  - R2: a war is won by aim only when the goal has changed hands to the attacker during the war. Holding it at the declaration does not count: the defender's last settlement there must be gone. A realm may still claim a region it holds where another people keeps a town. If it wins that war by the score, the other people's settlements there pass to it with the peace. Wars won by aim with no battle in one year: at most 3 in 100 default worlds.
- Interim choice: the rulings are specified and go into P3-10 before its merge (generator stays 0.19.0). Paul's decisions of 2026-11-20 stand in their intent: orcs still awaken with a quarrel and declare at shorter odds, and an attacker still wins by aim only by holding its goal. The rulings add a condition to each.
- Status: confirmed by Paul on 2026-11-22 (see "Paul's decisions of 2026-11-22"); listed as an open question in design.md section 12 (2026-11-21). Merged into design.md, 2026-11-23.

### History and names: P3-11 orc arrival, wording and names (scope)

- Date: 2026-11-21
- Raised by: spec writer (P3-11, `docs/specs/p3-11-followups.md`), from the world critic's notes N1 to N4 of the P3-10 second round, the second-round validation and Paul's decision of 2026-11-21 on faerie names
- Design doc section: 5 (the orc age; races and cultures: names), 6 (text rendered from records)
- Problem: after P3-10 (generator 0.19.0) the orcs sit still for a median 232 years after their awakening (88 of 100 default worlds have 200 silent years or more). Claims and foothold cessions name a region where only towns are at stake ("ceded the Graven Thicket" when one town passed). Faerie names keep their baby-talk sounds. `tard` and faerie names ending in `-ass` pass the filter. Small counts are written as digits beside words ("one other settlement", "2 other settlements").
- Proposal: one change, generator 0.20.0, schema 7 unchanged.
  - Orcs arrive in numbers: a race field `awakening_population` (orc 1,200) starts the first orc settlement larger, and its first colony is not held back by pressure (corrected the same day: the population alone did not make it ready, since an orc village's kind cap is 7,999), with the target that the median first orc colony, declaration, sack or capture comes within 100 years of the awakening.
  - Two appended event kinds name the settlements of a claim on a region the holder does not hold and of a foothold cession.
  - The faerie syllables follow Paul's decision (no `p` or `b` onsets, fewer `ee` and `ou`), and no name repeats a syllable twice in a row, in every culture.
  - `tard` is filtered anywhere in a word.
  - Values one to nine are written as words in sentences.
  - One open question, non-blocking: a war fought over another region than the claim it cites (because that region became weary, was lost, or became the defender's capital) is kept as specified, and the report names the claim's region; the alternatives are a fresh claim at the declaration, or no war.
- Interim choice: as proposed; the design document is not affected (the orcs' "coming opens the most recent age of conflict" is served, not changed; Paul's decisions on the orc age's length and on orcs at peace among goblins stand).
- Status: scope record; the question of section 10 item 1 was decided by Paul on 2026-11-22: a fresh claim, option (b) (see "Paul's decisions of 2026-11-22"). Merged into design.md, 2026-11-23.

### History: the P3-11 orc targets, the lead's ruling (awaiting Paul)

- Date: 2026-11-22
- Raised by: lead, from the P3-11 validation (`docs/reviews/p3-11-followups-validation.md`) and the world critic's review (`docs/reviews/p3-11-followups-world-critic.md`)
- Design doc section: 5 (history: the orc age)
- Problem: at generator 0.20.0 the orcs act a median 9 years after their awakening (232 at 0.19.0), declare almost twice as many wars and win 168 against 95 lost. Two limits the lead set in the P3-10 rulings are each missed by one world over 100 default worlds: 11 worlds end with exactly one standing orc settlement (limit 10), and in 3 worlds the orcs do nothing after year 5750 (limit 2). Validation found no defect.
- Ruling: both misses are accepted and P3-11 merges. The limits were set to catch broken or frozen orc realms. The critic read all fourteen worlds: the eleven are a rise and fall by the orcs' own wars or a conquest by others in the last decades, each leaving ruins and a remnant; the three silent worlds are realms of four to six settlements at peace, which Paul accepted on 2026-11-21. For later measurements the two limits are restated as the critic suggests: one standing orc settlement with no war won and nothing sacked (6 worlds at 0.20.0), and silent after 5750 with at most two standing settlements (0 worlds).
- Left for a later balance pass, with the next change that alters world bytes: the first orc war is mostly declared with exactly two settlements and is then lost more often than won (29 lost, 19 won), so the quarrel war should be gated on strength (critic's F1); `shiva` joins the famous-name list; `fag` inside a word; the dwarven codas `din` and `rin` that hide stutters such as Clan Didin.
- Questions for Paul (from the critic): (1) orcs now sometimes declare war on goblins (12 declarations in 9 worlds): is evil turning on evil welcome? (2) should faerie near-repeats such as Feefi and Flaefla also go (6.6% of faerie names)? (3) is an orc age that is a gamble between collapse and conquest the intended spread?
- Interim choice: as ruled.
- Status: confirmed by Paul on 2026-11-22; the three questions answered (see "Paul's decisions of 2026-11-22"). Merged into design.md, 2026-11-23.

### Paul's decisions of 2026-11-22: the orc rulings confirmed, and four questions answered

- Date: 2026-11-22
- Raised by: Paul, in the session, answering the lead's summary of what awaited confirmation
- Design doc section: 5 (history: the orc age, wars and how they end; races and cultures: names)
- Decisions:
  1. The lead's P3-10 rulings R1 and R2 ("History: the P3-10 orc and aim rulings") are confirmed.
  2. The lead's ruling on the P3-11 orc targets ("History: the P3-11 orc targets, the lead's ruling") is confirmed, with the restated limits.
  3. Orcs declaring war on goblins: welcome. "It makes sense that evil would attack evil sometimes." No change.
  4. Faerie near-repeats such as Feefi, Flaefla and Gliglia: fine as they are. No change.
  5. The orc age as a gamble between collapse and conquest: fine. No change.
  6. A war fought over another region than the claim it cites: a fresh claim is written on the new goal at the declaration (option (b) of `docs/specs/p3-11-followups.md` section 10 item 1), so every war cites a claim on its own region. This replaces the shipped reading (a), the note "(The claim, since ..., was to ...)".
- Interim choice: decision 6 adds an event per such war and so changes world bytes. It is scheduled with the next change that alters world bytes, together with the items of `docs/specs/p3-11-followups.md` section 12; until then generator 0.20.0 keeps reading (a) and the history report's note.
- Status: accepted; decision 6 to be specified and implemented with the next balance change. Merged into design.md, 2026-11-23.

### Paul's decision of 2026-11-22: regions renamed by history, and a history for the mountain peoples

- Date: 2026-11-22
- Raised by: Paul, in the session, on the two ideas of "History: two ideas from the world critic's review of P3-08"
- Design doc section: 4 (regions), 5 (history: the peoples, trade, monsters), 12 (open questions)
- Decisions:
  1. **History renames regions**: accepted. A region's defining event (a great sack, a long war, a founding, a plague) can give it its present-day name, with the old name kept in the record.
  2. **Mountain peoples get history of their own**: accepted, with Paul's direction for the dwarves:
     - They are great forgers. They make wealth and weapons, and trade them to the other kingdoms. This joins the trade and wealth already placed in P5 (Paul, 2026-11-20: "P5 is where the alliances and trade and wealth should go"), and the artifacts made in response to conflicts.
     - The mines hold great dangers. Delving deep can wake something that wipes a hold or a whole realm out, leaving a lost hold as a ruin and a site. (Paul's example is the Balrog of The Lord of the Rings; the game needs its own deep horrors, from the monster data of design section 5.)
  3. The share of grim region names is lowered in data before P5, with the next change that alters world bytes.
- Interim choice: 1 and 2 go to P5 History depth, to be specified there (events for delvings, forging, trade in arms, a woken danger and lost holds; a record of renamed places for the sites and quests of P6). 3 joins the items of `docs/specs/p3-11-followups.md` section 12.
- Status: accepted; design doc sections 5 and 12 to be updated. Merged into design.md, 2026-11-23.

### Paul's approval of phase P3 History core, 2026-11-22

- Date: 2026-11-22
- Raised by: Paul, in the session, after reading the history reports of the phrase world and seeds 5 and 7 (generator 0.20.0)
- Design doc section: 13 (phases; the human checkpoint)
- Decision: "they look great for this stage, ready to move to the next phase." P3 is approved. P4 Viewer v1 and P5 History depth start, in parallel.
- Carried into P5 (first stage, since it alters world bytes): the items of `docs/specs/p3-11-followups.md` section 12 (a fresh claim at a declaration, the quarrel war gated on strength, fewer grim region names, the `became_city` wording, name-filter additions, a stored hidden-refuge flag and a recorded claim drop). Carried with it, without world changes: the world critic's notes on the history report (`docs/reviews/p3-07-history-report-world-critic-r17.md` B1 to B6) and the audit's advisory A3.
- Status: accepted; design doc section 13's status line to be updated (P3 complete and approved on 2026-11-22). Merged into design.md, 2026-11-23.

### Viewer v1: the library answers the year, the viewer draws it

- Date: 2026-11-22
- Raised by: spec-writer (P4-00, `docs/specs/p4-00-viewer-v1-overview.md`)
- Design doc section: 10 (world viewer: first version), 12 (open question "Layer catalogue in the library"), 13 (phase P4)
- Problem: the design lists what viewer v1 shows (territory, settlements and sites as markers, a timeline scrubber, a browser for civilizations, figures and events with causes) but not which side computes it. Three readings are needed. (1) The viewer keeps its own copy of the layer catalogue's units and reads region names from a raw column (entry of 2026-11-18). (2) If the viewer computed holders by year, summary sets or event text itself, it could disagree with `report --history`, and every P5 change to a history table would need a second edit in the extension. (3) Sites arrive in P6 and figures in P5, so "site markers" and a figures browser have nothing to show in P4.
- Proposal: (1) `worldgen` exposes the layer catalogue (group, unit, decimal scale, special values, display form, legends) and the region name in its public interface (spec P4-02), and the viewer's copy goes. (2) `worldgen` gains a read-only timeline over the history tables (holders, settlement states, relations, phase and events at a year; events by entity; the events an event caused), a civilization colour rule (a hue family per race, shades per civilization) and a year-dependent composite `territory` with `worldgen-cli render --composite territory --year Y` (spec P4-01); the extension reads history only through the library and issues no SQL of its own on history tables; event text stays in `render_event` and `history_note`. (3) Until P6, "site markers" are ruins, passes and the places of the year's events; the browser lists figures once P5 provides them through the library.
- Interim choice: as proposed, in specs P4-01 to P4-05. No world byte changes; no generator or schema version bump.
- Status: accepted by Paul on 2026-11-22 with the P4 overview's recommendations. Merged into design.md, 2026-11-23.

### Viewer v1: the history movie is playback, recorded with Godot's Movie Maker

- Date: 2026-11-22
- Raised by: spec-writer (P4-00)
- Design doc section: 12 (open question "History movie"), 13 (phase P4: candidate addition)
- Problem: where the movie falls in P4, and whether saving it as a video is wanted (entry "Viewer: a history movie on the 2D map"). An encoder in the extension would be a new vendored dependency.
- Proposal: the movie is the last P4 stage (P4-06): play, pause, speed, skipping quiet years, and the year's sentences beside the map, on top of the scrubber, territory and markers. No video encoder is built: Godot's Movie Maker mode (`--write-movie`, AVI with MJPEG or a PNG sequence, fixed frame rate) records the viewer, and P4-06 adds command-line options to play a span and quit so that one command records a movie into the build root. If Movie Maker proves unusable, the fallback is a PNG frame dump, not an encoder.
- Interim choice: as proposed.
- Status: accepted by Paul on 2026-11-22 (P4 overview decision 2). Merged into design.md, 2026-11-23.

### History depth: the P5 overview's readings

- Date: 2026-11-22
- Raised by: spec writer (P5-00, `docs/specs/p5-00-history-depth-overview.md`)
- Design doc section: 5 (history: everything "waiting for phase P5"), 6 (world database), 9 (magic), 12 (sub-peoples, renamed regions, mountain peoples), 13 (the P5 and P6 rows), 14 (the divine script)
- Problem: the P5 row of design 13 lists ten items and an exit check, without an order or a split, and the design is silent or unclear on several points the stages share:
  1. Sites: design 13 puts sites in P6 ("derived from history"), but design 5 stores passes as sites and design 6 has `monster.lair_site_id` and `spellbook.site_id`, so history refers to sites before P6 makes them.
  2. Spellbooks: design 6 `spellbook` is "where copies can be found" (a present placement), design 13 puts spellbook placement in P6, design 5 has casters write them in history.
  3. `race_relation` and `beast_population` are "at the end year" in design 6, while the viewer's timeline (P4) needs state by year.
  4. A civilization is "a society of one race, or a mix" (design 5); P3's civilization has one race.
  5. Paul's deep dangers can "wipe a hold or a whole realm out"; design 5 says no race dies out.
  6. Design 13's P6 row lists "world-creation settings wired to the command line"; P3-01 has wired all of them already, the divine script excepted.
  7. Design 14 places the divine script after the prototype; P3 refuses `--divine-script` "until P5".
- Proposal (the overview's readings):
  1. Twelve stage specs in a fixed dependency order: P5-01 the carried-over follow-ups (P3-11 section 12 and the report notes B1 to B6 and A3), then beasts and monsters; race relations, mixed settlements and migrations; wealth, trade and alliances with mines and forges; roads and improved passes; figures; mountain depths; pantheon and interventions; casters and spells; factions; artifacts; regions renamed by history. One new subsystem per stage; merges serial, implementation in parallel where dependencies allow.
  2. History creates a `site` table in P5 (first with P5-02) for the places it acts on (lairs, mines, towers, marked shrines, lost holds); P6 adds derived sites and dungeon records. P5 records books written; the `spellbook` placement table is P6's.
  3. Tables by year beside the end-year tables of design 6, as `settlement_census`.
  4. A civilization keeps a principal race; the racial mix is per settlement (`settlement_race`).
  5. A deep horror may end a realm; if it held its race's last settlements, the survivors flee as with any last refuge.
  6. P5-01 records a refuge still hidden at the present and a dropped claim as appended event kinds, so it needs no schema change.
  7. Event kind ids and schema versions are assigned in merge order; specs name kinds by key.
  8. The history report moves to format v2 with P5-01; within v2, a stage may append a block for its entity kind before `[stats]` and lines at the end of a civilization block.
  9. `--monster-pressure` takes effect with P5-02, `--divine-activity` with P5-08; `--divine-script` stays refused through P5. Nothing of the settings is left for P6 but the script.
  10. Event rules stay code with TOML numbers.
  11. Budget: each stage adds at most 5 s at 1024; the whole default `generate` stays under 60 s at the end of P5 (about 9 s at generator 0.20.0).
  12. Exit check: "takes part" means a participant of at least one event; each race, camp and monster kind has a hard and a sweep target (P5-00 8.3); a bloc of allies and vassals is held to the runaway bound.
- Interim choice: as proposed; the stage specs follow them. Four questions change the split or order and are put to Paul (P5-00 11): sites in P5, gods late with all five intervention kinds, renamed regions early, no sub-peoples in P5 (hill dwarves an optional stage after P5-07, dark elves after underground geography).
- Status: accepted by Paul on 2026-11-22 (see "Paul's decision of 2026-11-22: the P5 overview's questions go with their recommendations"). Merged into design.md, 2026-11-23.

### Paul's decision of 2026-11-22: the P4 overview's questions go with their recommendations

- Date: 2026-11-22
- Raised by: Paul, in the session, after reading section 11 of `docs/specs/p4-00-viewer-v1-overview.md`
- Design doc section: 10 (viewer), 12 (open questions "Layer catalogue in the library" and "History movie"), 13 (P4)
- Decisions, each as recommended:
  1. `worldgen` exposes the layer catalogue and region names through its public interface (stage P4-02); event text stays in `worldgen`.
  2. The history movie is the last stage (P4-06), as playback of the year bar. No video encoder of the project's own: Godot's Movie Maker records it.
  3. The viewer opens at the present day (year E).
  4. Civilization colours: one hue family per race, a shade per civilization.
  5. The walkthrough world of the exit check is seed 42 at 1024.
- Status: accepted; design doc section 12's two P4 questions to be marked decided. Merged into design.md, 2026-11-23.

### Paul's decision of 2026-11-22: the P5 overview's questions go with their recommendations

- Date: 2026-11-22
- Raised by: Paul, in the session, after reading the open questions of `docs/specs/p5-00-history-depth-overview.md`
- Design doc section: 5 (history), 6 (world database: sites), 12 (open questions: sub-peoples, history renames regions), 13 (P5, P6)
- Decisions, each as recommended:
  1. Sites: stage P5-02 creates the `site` table for the places history acts on (lairs, mines, towers, marked shrines, lost holds). P6 adds the sites derived from the event log and the dungeon records.
  2. The gods come late, as stage P5-08, after monsters and figures, so that all five kinds of intervention arrive together.
  3. Regions renamed by history come right after P5-01 (stage P5-12 in the overview's numbering); which events can rename a region is set in data, so later stages add their own.
  4. Sub-peoples: none inside P5's stages; one culture per race. Hill dwarves may be a small optional stage after the mountain depths (P5-07); dark elves wait for underground geography.
- Status: accepted; design doc sections 12 and 13 to be updated. Merged into design.md, 2026-11-23.

### The layer catalogue is code, not TOML data

- Date: 2026-11-22
- Raised by: spec-writer (P4-02, `docs/specs/p4-02-layer-catalogue.md` section 3)
- Design doc section: 10 (tech stack, "Data in TOML"; `worldgen` public API), 12 (open question "Layer catalogue in the library")
- Problem: hard rule 4 puts hand-edited content (races, beasts, cultures, biomes, event rules) in TOML. P4-02 moves the layer catalogue of spec P1-00 section 4 (each layer's type, range, unit, display form and legend) into `worldgen`'s public interface. It could be a TOML data kind or code.
- Proposal: code, a fixed table in `worldgen/inspect/layer_catalogue.hpp`. The catalogue is not tunable content: it describes what the stages' code writes, and changing a fixed layer's type, units or meaning is already a schema change (P1-00 4 rule 3), so a TOML copy could only disagree with the code. The palettes and ramps it uses are code already, and readers (the viewer, later the create-world screen and the in-game map) then need no data root to label a layer. Labels that do depend on data (biome and region names and colours) keep coming from the world's own tables.
- Interim choice: as proposed, in P4-02.
- Status: open. Listed as an open question in design.md section 12 (2026-11-23).

### History and report: P5-01 carried-over follow-ups (scope)

- Date: 2026-11-22
- Raised by: spec writer (P5-01, `docs/specs/p5-01-followups.md`), from the items Paul's approval of P3 carried into P5 ("Paul's approval of phase P3 History core, 2026-11-22"; `docs/specs/p3-11-followups.md` section 12) and the world critic's notes on the history report (`docs/reviews/p3-07-history-report-world-critic-r17.md` B1 to B6)
- Design doc section: 4 (region names), 5 (history: wars and claims, the orc age, refuges), 6 (text rendered from records), 13 (the text history report)
- Problem: at generator 0.20.0 a war re-aimed at its declaration cites a claim on another region (280 in 100 default worlds); the orcs fight their first war with two settlements, a camp and a colony a few years old, and lose it more often than they win (27 won, 32 lost); 9% of drawn region names are grim; the history report cannot say which refuges are still hidden or which claims were dropped; battle tolls repeat to the man and mix digits and words; a flare-up can last one year; the report's summary floods some worlds with plagues and runs to 1,300 lines with four civilizations per race.
- Proposal: one change, generator 0.21.0, schema 7 unchanged.
  - A fresh claim on the war's goal, caused by the old claim, written at a re-aimed declaration (Paul's decision 6 of 2026-11-22).
  - The awakening quarrel claim is held back until the realm's strength against its neighbour reaches a fair fight: the declaration odds without the aggression discount at 1000 (`history.conflict.quarrel_strength_permille`). Reading taken here: "gated on strength" is read with the measure the rules already compute at every declaration, applied to the quarrel claim only, so every later orc war keeps the short odds Paul decided on 2026-11-20.
  - 34 of the 40 grim words left in the common region-name lists move to barren land; spirits and curses stay.
  - Appended event kinds 41 `claim_dropped` and 42 `refuge_hidden` (a span over the refuge's hidden years) record what the world did not, as the P5 overview planned (events, not columns).
  - Battle tolls get a keyed factor of 80% to 120%; no flare-up shorter than 10 years; the `became_city` sentence without its figure; five name-filter entries; one number form per clause.
  - The history report becomes format v2, with P5's growth rule.
- Interim choice: as proposed; the design document is not affected. Questions for the lead (P5-01 section 10): the delay the strength gate puts on the first orc war; the `civs4` summary length (about 1,100 lines, against "about 1,000" for default worlds); keeping the two optional rules (tolls, short flare-ups).
- Status: open

### Paul's decision of 2026-11-22: the P5-01 critic's three notes go to P5-12

- Date: 2026-11-22
- Raised by: lead, from the world critic's review of P5-01 (`docs/reviews/p5-01-followups-world-critic.md`, F1 to F3)
- Design doc section: 13 (P5)
- Problem: at generator 0.21.0 some settlement names double a place suffix (`Sudsteadstead`, `Galwickwick`, `Stonton`, `Deasstead`), one region is named "Tarden" (the name filter does not cover region names), and the `refuges` list of the report's `[present]` section also lists refuges found long ago. Fixing names changes world bytes, so every reference and golden value would be recorded twice in a row.
- Decision (Paul: "carry them into P5-12"): P5-01 merges as it is; the three notes are fixed in P5-12 (regions renamed by history), the next stage that changes world bytes.

### Paul's decisions of 2026-11-22: the report length target restated, the quarrel threshold kept

- Date: 2026-11-22
- Raised by: lead, from the P5-01 validation sweep at generator 0.21.0 (`docs/specs/p5-01-followups.md` section 8, T18, T12 and T8; section 10 questions 1 and 2)
- Design doc section: 5 (the orc age), 13 (the text history report)
- Problem: T18 asked for at most 700 summary lines in every default world and at most 1,100 in every world with four civilizations per race; measured at 0.21.0, 100 default worlds give 321 to 754 lines (median 571) and 20 `civs4` worlds 890 to 1,353 (median 1,046). The orcs' strength gate delays their first declaration to a median 166 years after the awakening (59 at 0.20.0), in 81 of 100 worlds.
- Proposal: (1) "Restate the report length target": summary length, default worlds median at most 700 and maximum at most 800 lines; four civilizations per race median at most 1,100 and maximum at most 1,400. (2) `history.conflict.quarrel_strength_permille` stays 1000; no lower value is measured.
- Interim choice: P5-01 r1 section 8 and P3-07 r20 25.5 state T18 so; P5-01 questions 1 and 2 are closed. The design document is not affected.
- Status: accepted

### History: regions renamed by history, P5-12 (readings and scope)

- Date: 2026-11-22
- Raised by: spec writer (P5-12, `docs/specs/p5-12-renamed-regions.md`), from Paul's decision of 2026-11-22 ("regions renamed by history", decision 1), his decision on the P5 overview's question 3, and "the P5-01 critic's three notes go to P5-12"
- Design doc section: 4 (region names), 5 (history), 6 (world database), 12 (open question "history renames regions"), 13 (the text history report)
- Problem: the design says a region's defining event can give it its present-day name, with the old name kept, but not which events define a region, how often, how the name is built, which name each reader shows, or how sentences name a region that was renamed later. The P5-01 review also left three name and report notes for this stage.
- Proposal (P5-12's readings):
  1. A subsystem after conflict (steps T6 and Q4) reads the log; **rules in data** (`history.renaming`: kinds, value keys summed over a window, a least salience, a capital condition, a keyed chance, a weight, the forms) decide which events can rename, so later stages add their own kinds as data. Shipped: the fall of a realm, the sack of a capital, a great sack (1,500 dead), a slaughter in the region's battles (5,000 dead within a century), a great plague (3,000 dead), a realm's seat becoming a city.
  2. **Rarity**: at most two renamings per region, 300 years apart, the second for a weightier deed; at most 15% of a world's regions; at most one per century per 20 regions. Measured on the 0.21.0 sweep: a median of 4 of 49 regions at 1024.
  3. **Names** from English forms with the deed's people or place (`Edadbury's Fall`, `The Grave of Cear`, `The Red Wood`, `The Heart of Nife`), unique in the world, through the placename checks and the name filter at the joins; the namer's culture is recorded.
  4. **`region.name` stays geographic**; the new names are rows of a new table `region_name`. Sentences name a region as at the event (an old event keeps the old name); references outside sentences in the history report use the present name; the layer catalogue's `region_name()` keeps the geographic name; the timeline answers the name at a year, and the viewer's hover text follows the shown year.
  5. F1: a settlement root may not end in a fused place suffix of its kind (or its first syllable), nor in the suffix's first letter; the human syllables forbid `ass`. F2: the name filter applies to region names at the joins of their parts only. F3: the report's `[present]` lists only the refuges still hidden at E. The history report becomes format v3.
- Interim choice: as proposed. Questions for Paul (P5-12 17): the share of renamed regions; the wider F1 rule and V8's false positives (`Pinegrove`); English forms only, with the culture recorded for later per-culture lists.
- Status: open. Listed as an open question in design.md section 12 (2026-11-23).

### Schema 8: table `region_name` (P5-12)

- Date: 2026-11-22
- Raised by: spec writer (P5-12 7.2; P3-04 r21 23.4), hard rule 7
- Design doc section: 6 (world database), 12 (open question "history renames regions")
- Problem: renamed regions need a record of every name a region bore, with its year, its culture and the event that gave it (P5-00 6.1 planned it), and history must not rewrite geography's `region.name` (P3-00 7).
- Proposal: schema 7 → 8 with generator 0.22.0: `CREATE TABLE region_name (region INTEGER NOT NULL, from_year INTEGER NOT NULL, name TEXT NOT NULL, culture INTEGER NOT NULL, event INTEGER NOT NULL, PRIMARY KEY (region, from_year)) STRICT, WITHOUT ROWID`, executed after `relation`, written by the history stage; readers refuse version 7; the empty world grows by one page. Appended event kind 43 `region_renamed` (not a schema change by itself). `docs/specs/schema.md` updated. The schema number is the next free one at merge (P5-00 4.3); P5-12 merges straight after P5-01, so it is expected to be 8.
- Interim choice: as proposed.

### Viewer v1, P4-03: the year readout at the present, and keys in a C++-only screen

- Date: 2026-11-23
- Raised by: viewer engineer, implementing the viewer part of P4-03
- Design doc section: 10 (world viewer)
- Problem: (1) P4-03 4.4 leaves the age out of `%YearLabel` "when age is 0", and its check H2 takes the age from the report's `[ages]` line covering Y, which covers no line at E. But P4-01 section 4 puts the present in the last age, and the timeline gives that age at E, so the viewer reads "Year 6000 of 6000 (the present)   |   The Age of Orcs   |   no phase" while H2 as written expects no age part. (2) Writing the screen in C++ only, Godot's GUI focus navigation consumes the arrow keys (`ui_left`, `ui_right`) whenever a focusable Control has keyboard focus, before `_unhandled_key_input` sees them, so the year keys of P4-03 4.3 work only while no focusable control holds focus. A GDScript screen would hit the same; nothing in godot-cpp is missing. (3) Minor C++ friction: `godot::String + "literal"` is ambiguous in some expressions (a conditional operator with a `const String&`), so literals have to be wrapped in `godot::String(...)`; `Image::ptrw()` (godot-cpp low-level API) lets the territory image be redrawn in place without a copy.
- Interim choice: (1) the viewer follows the timeline (the age of the present is the last age) and the check takes the last `[ages]` line at E; P4-03 4.4 and H2 should say so. (2) The year buttons, the slider and the map view have `focus_mode` none in `viewer.tscn`; the layer list and the meta tree keep their focus behaviour, and while one has focus the arrows act on it. A later screen may read the year keys in `_input`, skipping them while a text field or dialog has focus, if the focus behaviour proves annoying in the walkthrough.
- Status: open. Listed as an open question in design.md section 12 (2026-11-23).

### Viewer v1, P4-04: markers are a library answer, with a CLI overlay

- Date: 2026-11-23
- Raised by: spec writer (P4-04, `docs/specs/p4-04-markers.md`)
- Design doc section: 10 (world viewer: "settlements and sites as markers"), 13 (P4 row)
- Problem: P4-00 3.1 had P4-04 draw markers in the extension and add nothing to `worldgen`. But which settlement is a capital in a year, when and by what a ruin fell (cleared from the row once it fades), where an event happened and which symbol it gets are rules over the history, and without a library answer the viewer would restate them (against V3) and no marker image could be made or compared without the viewer. The design also says nothing of marker shapes, sizes or of sites before P6.
- Proposal: `worldgen/inspect/markers.hpp` answers the markers of a year (settlements with kind, status, owner, population and capital; ruins with their year and event; event places with a symbol by kind key; passes) and defines integer glyph grids and a rasteriser; `worldgen-cli render --markers [--faded] [--year Y]` draws them over any layer or composite (P0-04 amendment 24), and `world-hashes.sh` compares the marker images across compilers. The viewer draws the same glyphs at screen size. Shape and size by kind rather than by a separate population band; one coloured ring per event place; site and figure markers reserved in the API for P5-02 and P5-06.
- Interim choice: as proposed, in P4-04 (open questions 1 to 5 there). No world byte changes; no generator or schema version bump.
- Status: open. Listed as an open question in design.md section 12 (2026-11-23).

### History: P5-01's T16 restated as a rate (P5-12)

- Date: 2026-11-23
- Raised by: lead, from the engineer's sweep of P5-12 at generator 0.22.0 (`docs/specs/p5-12-renamed-regions.md` r2 section 11, "T16 at 0.22.0"; `docs/specs/p5-01-followups.md` section 8, T16)
- Design doc section: 5 (history: wars and battles), 13 (P5 validation targets)
- Problem: P5-01's target T16 bounds the battles that repeat an earlier battle's sides and both tolls exactly at **at most 10** over the 100 default 1024 worlds (measured 9 at 0.21.0). P5-12 changes no battle rule, but its renamings shift the event ids that conflict keys its draws by, and the engineer measured **13 of 8,935 battles** at 0.22.0. A fixed count grows with the number of battles a sweep holds, so it measures the sweep as much as the rule.
- Proposal: restate T16 as a rate: **at most 2 repeated battles per thousand battles** of the sweep (13 of 8,935 is 1.5‰; 0.21.0's 9 was about 1‰). The second half of T16 (no battle sentence with a digit toll and a word toll in one clause: 0) is unchanged.
- Interim choice: none; the bound of 10 is not changed by the spec, and validation reports T16 as missed at 0.22.0 until Paul decides.
- Decision (Paul, 2026-11-23): "2 per thousand sounds pretty low, should be fine." T16 is at most 2 repeated battles per thousand battles of the default sweep; validation measured 13 of 8,935 (1.45 per thousand) at 0.22.0, so the target is met. The P5-02 spec states it so.

- Status: decided

### History: beasts and monsters, P5-02 (readings and scope)

- Date: 2026-11-23
- Raised by: spec writer (`docs/specs/p5-02-beasts-monsters.md`)
- Design doc section: 5 ("Peoples, beasts and monsters", "Beasts as pressure", "Monsters as actors", "Undead from history", "Flare-ups"), 6, 13
- Problem: the design gives beasts and monsters roles in history but no rules; P5-00 3.1 outlines the stage.
- Proposal (readings taken): beasts are counts per region and species that fill their habitat and retreat before people; predators slow and game speeds the **rate** of settlement growth only (never the size a settlement tends to), so P3's balance holds; no predator-prey coupling yet. Monsters are rare named actors created by one subsystem after renaming: wild kinds appear by habitat, the undead rise where at least 4,000 died within a century; they raid within a territory of borders, sleep and wake, age, may overrun one small settlement and make it their lair, and are slain or driven off by the realms they raided (no heroes before P5-06; a failed hunt costs nothing). Monster names are built from the syllables of a culture named in the monster data with the kind's own forms (monsters have no culture of their own; P5-00 4.5 reads "templates in each culture"), outposts from a new culture field `site_forms`. The waking of a great monster (`flare` kinds: dragons, giants) in a quiet step is a tension source; such a flare-up forces no war, so design 5's "a flare-up always brings its war" holds for flare-ups of strain only. Raids, sleep, hunts and risings happen in tick years only; quiet steps write appearances and deaths of age. `param.event_intensity` does not scale monsters.
- Interim choice: as proposed.

### Schema 9: beasts, monsters and sites (P5-02)

- Date: 2026-11-23
- Raised by: spec writer (P5-02 11), hard rule 7
- Design doc section: 6 (`beast_population`, `monster`, `site`)
- Problem: P5-02 needs the beast and monster definitions in the world, beast counts by year, named monsters and the first sites, and a record of how each dead settlement ended.
- Proposal: schema 8 → 9 with generator 0.23.0 (numbers the next free at merge): tables `beast`, `monster_kind` (echoes of the data, as `biome` and `race`), `beast_population` (region, beast, year, count; rows at year 0, at every phase end and at E − 1 when the count changed), `monster` (design 6 columns plus `born_event`, `death_event`, `status`, `hoard`), `site` (design 6 columns without P6's `layout_seed`, plus region, settlement, founder, monster, `ended_year`, `ending_event`, `ending`); column `settlement.ending` (how a dead settlement ended, derived from its `ruin_event` and checked on read). Entity types 8 `monster` and 9 `site` come into use. Event kinds 44 to 55 appended. Readers refuse version 8.
- Interim choice: as proposed.

### Paul's wish of 2026-11-23: abandoned places to explore (P5-02)

- Date: 2026-11-23
- Raised by: Paul (relayed by the lead), answered by the spec writer in P5-02 8
- Design doc section: 5 ("From history to play"), 6 (`site`)
- Wish: "Are there any conditions that can lead to an area of the map losing its owner without gaining a new one? I'd like there to be situations that can lead to abandoned points of interest that the player can explore." At 0.22.0 the only abandoned place is a ruin burned in a war (of 1,123 dead settlements in 100 default worlds, 1,071 sacked, 52 left by migration).
- Answer in P5-02: (a) a monster may **overrun** a small settlement and make it its lair: the settlement dies (ending `overrun`), the region is released when it was its owner's last there, and no realm founds a settlement in it while the monster lives; (b) a **ghost rule**: a hamlet or village struck below 300 by famine or plague may be abandoned and left standing, and every dead settlement records **how it ended** (`settlement.ending`: sacked, overrun, plague, famine, left) beside the unchanged status; (c) the first **sites**: lairs, and **watchtowers** raised on a frontier in war and left when the land is lost, the realm falls or the frontier is quiet, each with founder, years, ending code and ending event; (d) the timeline answers by year which sites stand, which monsters live and how each dead place ended. Targets: a median of at least three ghost places per default world, a monster-held place open at E in at least half of the worlds, standing settlements at E at least 95% of 0.22.0's.
- Later stages add: mines and forges worked out or lost (P5-04), lost holds and delvings (P5-07), shrines marked by gods (P5-08), towers and libraries (P5-09), each as a `site` kind with its own ending codes; P6 derives battlefields, barrows, tombs and dungeon records.
- Interim choice: as answered.
- Measured at the first calibration (engineer, generator 0.23.0, 100 default 1024 worlds): a lair on a former settlement open at the end in 44 worlds; overruns median 1 a world; a raid took the largest settlement at the nearest hop, so its targets were 67% towns, 8% fortresses and 18% villages, and only small non-capital places can be overrun.
- Decision (Paul, 2026-11-23): "44% is a bit low, I'd expect among all the settlements that exist this would be a more likely outcome than that. I'd say the monsters would attack smaller settlements preferentially as easier targets actually". P5-02 r1 6.4.1: a raid's target is a keyed weighted draw among the settlements in reach, with weights falling with the settlement's kind (hamlet, village, town, city, fortress), with each border crossed and for a capital; a kind's might raises its weight for towns, cities and fortresses, so dragons and giants still strike towns. Might does not widen what a kind may overrun (its `overruns` list). Targets restated: a lair on a former settlement open at the end in at least 65 of 100 default worlds, a living monster's lair at the end in at least 85, overruns median 2 to 4; standing settlements at the end at least 92% of 0.22.0's median (population stays 95%). Raid deaths are expected to fall (smaller targets lose fewer).

- Status: decided

### Paul's decisions of 2026-11-23 on P5-02's open questions

- Date: 2026-11-23
- Raised by: lead, recording Paul's answers to the open questions of `docs/specs/p5-02-beasts-monsters.md` r0
- Design doc section: 5 (flare-ups; beasts and monsters; sites), 13 (P5 validation targets)
- Decisions, in Paul's words "1. I accept the narrowing, 2. That's fine to drop, 3. Now is good if it's easy now, 4. And 5. I take the recommendation":
  1. A flare-up started by a monster (a dragon or giant appearing in a quiet stretch) forces no war; design 5's "a flare-up always brings its war" holds for flare-ups of strain between realms. The design doc should say so.
  2. If the `civs4` summary median passes 1,100 lines, the monster lines are dropped from the summary on those worlds (they stay in the full report); the bound is not raised.
  3. Outposts (watchtowers) are built in this stage, if that is easy; if it proves costly the engineer says so and they move to a later stage.
  4. How a settlement ended is a recorded cause (column `settlement.ending`), not a new settlement status.
  5. Ancient renamings: no change in this stage; later, death-toll thresholds relative to the population of the time.
- Also decided the same day: the territory colours of P4-03a stand ("the colors look good to me"); the share of renamings by capital sack stays ("if the capital is sacked the conquerors will want to rename it").
- Not decided by Paul (the lead's interim): in P4-04 the spec writer's five open questions take their recommended choices until Paul answers them.

- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).

### Viewer v1, P4-05: the report prints its civilization blocks from the lines the browser shows

- Date: 2026-11-23
- Raised by: spec writer (P4-05, `docs/specs/p4-05-browser.md`)
- Design doc section: 10 (world viewer: "a browser for civilizations, figures and events with their causes"), 13 (P4 row and exit check)
- Problem: the browser must show a civilization's block "as the history report shows it", with links, and must not parse report text (P4-00 V3). The report builds those lines in private code, so a second copy in the viewer, or in a browser module, would drift from the report.
- Proposal: `worldgen` builds each block as structured lines (literal, text, entity reference, event reference, sentence), and the report prints its `[civilizations]` blocks from them (report bytes unchanged). Sentences get a linked form (`render_event_linked`), with `render_event` as its text. A browser index answers the civilization list, filtered event lists, event details, search by name and the place of an event. Event kind groups are a library table by key. Monsters, sites and figures become further lists of the same form when P5 provides them.
- Interim choice: as proposed, in P4-05 (open questions 1 to 6 there). No world byte changes; no generator or schema version bump. The P4-04 world critic's F3 and F5 re-record `p4-04-markers.txt`.
- Status: decided (Paul's answers of 2026-11-24 and the approval of P4). Merged into design.md sections 5 and 12 (2026-11-25).

### Godot: a large image that changes in place must be drawn as tiles (found in P4-05)

- Date: 2026-11-23
- Raised by: lead, from the viewer engineer's measurement (P4-05 prelude)
- Design doc section: 10 (world viewer), and any in-game map
- Problem: Godot 4.7 has no partial update of an `ImageTexture`: `update()` uploads the whole image, and an RGB8 image is converted to RGBA8 on the way. The viewer's 1024 by 1024 territory picture was one texture, uploaded whole on every year change; with a real renderer the slowest change took 170 ms against the 50 ms budget (headless: 6 ms, so only a run with a renderer shows it).
- Proposal: any large picture that changes in place (the viewer's map, later the in-game map with its fog and markers) is drawn as tiles, each its own RGBA8 texture, and only tiles whose pixels changed are uploaded. Speed checks of such a picture run with a renderer, not headless.
- Interim choice: the viewer does this (`LayerMapView`, 128 by 128 cell tiles; the slowest year change is 21 ms, 34 ms to the next drawn frame, under a virtual display). Nothing in the design document changes; this is a note for the game's map.
- Status: decided (Paul's answers of 2026-11-24 and the approval of P4). Merged into design.md sections 5 and 12 (2026-11-25).

### Paul's decisions of 2026-11-23 on P5-02 (undead in ruins, summary length)

- Date: 2026-11-23
- Raised by: lead, from the integration of P5-02 and its validation
- Design doc section: 5 (history: monsters), 13 (P5)
- Problem: (1) the P5-02 spec r1 said a monster may overrun a settlement only if its lair is not a settlement; the code applied that only to a lair won by an overrun, so an undead host that rises in a ruined settlement may still overrun one living settlement, and the accepted sweep figures were measured so. (2) The summary history report of worlds with four civilizations per race has a median of 1,101 lines on the spec's 20 worlds against T18's bound of 1,100 (1,089 on 30 worlds).
- Decision (Paul, 2026-11-23): (1) "Keep that, makes them more scary." Spec r2 section 5 says so: `holds_settlement` is set only by an overrun. (2) "Fine": the one-line miss is accepted; T18's bound for that set is read as about 1,100.
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).

### History: race relations, mixed settlements and migrations, P5-03 (readings and scope)

- Date: 2026-11-23
- Raised by: spec writer (`docs/specs/p5-03-race-relations.md` r1)
- Design doc section: 5 ("Gods and the order of the races": alignment sets starting relations; "Peoples, beasts and monsters": stances, "Race relations", "Mixed settlements"; "Waiting for phase P5": migrations of whole peoples as refugees; "The plan for phase P5": mixed peoples, state by year), 6 (`race_relation`, `settlement_race`), 9 ("Disposition")
- Problem: the design names race relations, mixed settlements and refugee migrations but gives no rules; P5-00 3.1 outlines the stage.
- Proposal (readings taken): every pair of races has a disposition = the camp table + half the sum of the two races' own `stances` (a new race field; shipped: elves and faeries +200 each way, dwarves and goblins −100 each way) + a memory that wars, conquests, sacks, captures and expulsions lower, shelter given raises, and that returns toward 0 at 40 a century, bounded at ±250. Three bands (hatred at or below −500, friendship at or above 150, wariness between, with a hysteresis of 50); a change of band is an event, and so is the first meeting of two peoples (realms within three borders). Realms read their peoples' disposition in place of the camp table (minus their own grudge, as before), and an `enmity` row opens when two peoples come to hate each other, so old wars shape the choices of realms founded later. Race-level memory is kept apart from P3's grudge between two realms. A civilization keeps one principal race; each settlement records the share of each race and its standing there (ruling, accepted, tolerated, subject): conquest and cession keep the conquered as subjects under the new rulers (with a tenth of the town the conquerors' own people), refugees bring their race, colonies are of the founder's people only, and minorities pass slowly into the ruling people (a tenth of their share a century). People P3 leaves lost (no room, or no own settlement within reach, or driven out) seek shelter in another people's settlement within two borders whose people's disposition toward them is at least −200, nearest first, kin before strangers; they never found a settlement of their own (P3's flight of a last refuge stays the only such founding). A ruling people in hatred with a minority of at most half the town may expel it. `temperament` is not needed (aggression carries it). Trade as a path into mixed settlements waits for P5-04.
- Interim choice: as proposed. Open for Paul in the spec (28): whether realms read the disposition now (recommended) and whether expulsions stay (recommended).
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).

### Schema 10: race relations, settlement races and phrases (P5-03)

- Date: 2026-11-23
- Raised by: spec writer (P5-03 16), hard rule 7
- Design doc section: 6 (`race_relation`, `settlement_race`, `monster`)
- Problem: P5-03 needs dispositions per pair of races and the mix of each settlement by year, and the P5-02 follow-ups render raid and boldness sentences by monster kind and beast, which the world must carry.
- Proposal: schema 9 → 10 with generator 0.24.0 (the next free at merge): tables `race_relation` (race_a, race_b, year, disposition, band; rows at the meeting, at each turn, at phase ends when changed and at E − 1; the design's end-year table is each pair's last row, as `beast_population` holds its end state) and `settlement_race` (settlement, race, year, share, standing; rows when a settlement's mix or standings change, at phase ends and at E − 1); columns `beast.bold_phrases` and `monster_kind.raid_phrases` (the data's phrases joined with `|`). `settlement.ending` gains code 6 (fled for fear of a monster; a value, not a column). Event kinds 56 to 62 appended. Readers refuse version 9. With a split into P5-02a and P5-03 (P5-03 0), the columns are schema 10 and the tables schema 11.
- Interim choice: as proposed.
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).

### History: P5-02's follow-ups in P5-03 (readings)

- Date: 2026-11-23
- Raised by: spec writer (P5-03 part B), from `docs/reviews/p5-02-beasts-monsters-world-critic.md`
- Design doc section: 5 ("Monsters as actors", "Beasts as pressure", "From history to play")
- Readings taken: repeated raids are answered by hunts called sooner (the chance grows with every raid a realm remembers) and by a small place's flight for fear after three raids in fifty years (a new event and a new ending, `fled`); tribute to a monster waits for wealth (P5-04). A raid cites the monster's last raid on the same place. Tolls above a kind's cap grow at a quarter rate instead of stopping; the undead and wyverns take no plunder. Raid and boldness sentences vary by monster kind and beast (phrases in data, chosen by the event id). The summary always tells the three most destructive monsters' stories (appearance, worst raid, overrun, end) inside its budget, and lists the worst ended ones. Abandonment goes only to a settlement within two borders, else its people scatter (and may find shelter abroad, part A), and loses 5% to 20% on the road. Ruins left standing (overrun, plague, famine, left, fled) never fade, and no ruin fades while an open lair holds it; burned ruins fade as before (P6 can still derive old foundations from faded rows). Monster epithets do not repeat within a world while unused ones remain; undead hosts are named for the place they rose from; `bird` joins the name filter; the ravage form `The Hollow of` becomes `'s Waste`. Bold beasts are drawn by their menace, so bears and boar act too. An overrun's chance falls with the target's population; monster pressure also scales overruns and the slaughter that raises the dead. Named slayers, heroes and gods in monster stories wait for P5-06 and P5-08.
- Interim choice: as proposed; Paul's open points are in the spec (28 items 3 and 4).

### Viewer v1, P4-06: how the history movie keeps time

- Date: 2026-11-23
- Raised by: spec writer (P4-06, `docs/specs/p4-06-history-movie.md`)
- Design doc section: 10 ("Viewer v1 decisions": "History movie"), 13 (P4 row and exit check)
- Problem: the design asks for play, pause, speed and skipping quiet years, with the year's sentences beside the map, recorded with Godot's Movie Maker. It does not say what "quiet" is, what a movie of one civilization plays, what happens when the speed asks for more years than frames, or how a recording stays the same from run to run.
- Proposal: playback walks the browser's current event list (a selected civilization's story by default span, first to last event). Each year has a dwell of 1/speed seconds; with "skip quiet years" on, a year with nothing listed takes a tenth of that. A frame makes at most one year change and never passes a year with a listed event, so every such year is shown with its sentences and flashing places, and busy stretches run slower than the speed. Time comes only from the frame step, so Movie Maker's fixed frame rate gives the same years in the same frames on every run. The sentences appear in a ticker over the map's lower edge; the year slider moves to a row of its own with the play controls. No library change.
- Interim choice: as proposed (P4-06 open questions 1 to 7).
- Status: decided (Paul's answers of 2026-11-24 and the approval of P4). Merged into design.md sections 5 and 12 (2026-11-25).

### History: P5-03 r2, readings as built and calibrated

- Date: 2026-11-24
- Raised by: spec writer (`docs/specs/p5-03-race-relations.md` r2), from the build, the calibration and the lead's rulings on `p5/03-int` (generator 0.24.0)
- Design doc section: 5 ("Race relations", "Mixed settlements", "Waiting for phase P5"; "Monsters as actors"), 9 ("Disposition")
- Problem: the readings of the two P5-03 entries above were taken with r1's estimated values; calibration moved several, which changes how the readings play out.
- Readings now in force: a pair's memory is bounded at ±500 and returns toward 0 at 20 a century, and each war, conquest, sack, capture, expulsion and shelter moves it three times r1's amount; peoples meet when their realms lie within eight borders (in practice every pair that can reach the other); an abandonment's survivors go only to a settlement of their realm in the same region, else they are displaced and may find shelter with another people; a small place flees a monster after two raids in fifty years; hunts grow more likely by a small step per remembered raid. The report counts a settlement as mixed at the end by a fixed share of 100 per mille, independent of the tuning. A rendered event sentence always starts with a capital letter. With these values a default world has a median of one change of band between peoples: most pairs keep the feeling their camps give them.
- Interim choice: as built. Open for Paul in the spec (28): whether relations should move more (question 7, after the world critic's review) and the bound for lairs on former settlements (question 8).
- Status: decided (Paul's answers of 2026-11-24 and the approval of P4). Merged into design.md sections 5 and 12 (2026-11-25).

### History: P5-03a relations follow-ups (readings)

- Date: 2026-11-24
- Raised by: spec writer (`docs/specs/p5-03a-relations-followups.md` r1), from the P5-03 world critic's review (F1 to F10), validation, the audit (A1, A2) and Paul's direction of 2026-11-24 (friendlier relations more often)
- Design doc section: 5 ("Race relations": "each pair of races has a starting disposition that events move over time"; "Deep time": "the first wars and alliances form"; "Waiting for phase P5": "aligned peoples in contact form partnerships"; "Monsters as actors")
- Problem: the design names what moves a disposition only by example (old wars leave grudges). At 0.24.0 only war's events and shelter move it, so dispositions stay at their camps' bands for thousands of years and turn only toward hatred; peoples sealed in by closed borders never meet; the undead rise almost only late.
- Readings taken: three things warm a pair of peoples besides shelter given: long peace between neighbours (goodwill builds after a hundred years without war between them, up to a cap, while their lands touch), a common enemy (a realm at war with both, or a monster raiding both, within a hundred years), and shelter counted at twice P5-03's value. A turn names the causes of its own direction ("After long years of peace", "After wars against a common enemy"). Peoples whose lands lie within three borders across closed borders (no walkable crossing, no pass) have word of each other after three hundred years and meet "by word"; armies and refugees still need open borders. The undead threshold follows the size of the world (a fortieth of the world's people dead in one region within a century, at least 1,000). A realm expels a people at most once in sixty years. No schema change: the new facts are values of existing event kinds.
- Interim choice: as proposed; open for Paul in the spec (17): how warm peace makes neighbours, meetings by word, the undead threshold, the summary's milestones, the expulsion gap, T18's maximum.
- Status: open.
- Engineer's readings (implementation, 2026-11-24): the templates of `disposition_turned` and `refugees_arrived` with the cause words of 3.5 and 7.3 are longer than the 512-byte limit of `history.report`'s `template` (P3-10's reading); the limit is raised to 1024 bytes. A contribution of zero units (a memory field tuned to 0) is kept by neither list of 3.5. The new event values' range checks (10.4) report `corrupt table 'event': row <e>: value <k> <v> outside <a> to <b>`, with `years` bounded by E - 1.
- Defect fixed (pre-existing since 0.24.0): a host named for its place (P5-03 12.2, "the Host of the Hushed Sweep") has no root, and the renaming rule `ravage` filled `{name}` with the empty string, giving region names such as "'s Shadow" (16 of 100 default summaries at 0.24.0). Reading: a monster with no root gives the rule no `{name}`, as a missing role does (P5-12), so only the rule's forms without `{name}` or `{stem}` can be chosen ("The Forsaken {noun}"); when none can be filled the rule does not rename. `RenamingSubsystem::build_name` in `src/history/renaming.cpp`.
- Calibration (engineer, 2026-11-24; lead's rulings of the same day; 1024 defaults, seeds 0 to 99, and `--civs-per-race 4` seeds 0 to 29): peace between neighbours as specified (`peace_years` 100, `peace_memory_per_century` 60, `peace_memory_max` 300); meeting by word at `meet_sealed_hops` **5** and `meet_sealed_years` **150** (was 3 and 300: three borders left 15 worlds with a pair that never met although a chain of borders joined their lands); and `history.conflict`'s `border_friction` **200 -> 300**. Reading: warmer neighbours fight less, which is the point, but it took about three wars a world (25.7 to 22.5 mean, most from goblins against dwarves and elves); `peace_years` 200 or a slower peace (40 a century) gave the wars back only in part and moved most turns past 5500, and keeping peace from lifting a pair out of hatred gave back almost none on a first sample of 13 worlds (the largest loss is dwarves against goblins, a wary pair). `border_friction` adds the same drive to every pair in contact, so it restores most where peace took (a pair at D = +50 has drive 250 instead of 150). It is also the awakening quarrel's limit (a people with disposition at or above it is not a target); no pair of a newly awakened realm reaches 300 there. Results: wars median 25 (mean 27.0; 0.24.0: 25.5, mean 25.7); T16 1.65 per thousand; 33% of turns into friendship; friendship and hatred at E medians 3 and 4. Population at E: median 211,977 against the bound 214,500 (0.24.0: 217,827). Not expulsions (2.8 a world against 3.6, 1,350 people against 1,990) or flights; the variants of the sweep ranged from 209,500 to 220,200 with no rule of this stage clearly behind the difference, the conquests are more often captures than sacks (9 against 6 a world), and the good peoples hold a little less; no defect found. Proposed: restate G2's population bound to 205,000. RR1 and RR3 restated (lead): pairs whose lands are joined by any chain of borders meet in 99 of 100 worlds (the one exception: humans and orcs seven borders apart, both late); pairs with no land path (24 pairs in 4 worlds, a race alone on its land mass) are reported, not counted. MX3's maximum is restated to 15 (13 measured).

### History: alliances and trade, P5-04 (readings and scope)

- Date: 2026-11-24
- Raised by: spec writer (`docs/specs/p5-04-alliances-trade.md` r1), from Paul's direction of 2026-11-24 (more alliances forged) and the lead's scope ruling (alliances the centre, trade and wealth thin)
- Design doc section: 5 ("Deep time": "the first wars and alliances form"; the race table: faeries "allies of the elves by origin"; "Waiting for phase P5": "aligned peoples in contact form partnerships that strengthen both, and an ally may join a partner's war"; "The plan for phase P5": "Dwarves are great forgers", the exit check on blocs), 6 (`relation`, `site`)
- Problem: the design names alliances, trade, wealth and vassalage without rules; P5-00 3.1 put all four, with mines and forges, into P5-04.
- Readings taken: (1) an alliance is a symmetric `relation` row of kind `alliance` between two realms within three open borders, not lately at war, whose disposition (peoples' disposition less the realms' grudge) is at least −300, formed by a keyed yearly draw in active phases when a reason holds (a common enemy, fear of a great realm or bloc, long trade, friendship), at most three allies a realm; renewed every 150 years while a reason holds, else it lapses; broken by cooling below −350 or two refused defence calls; (2) elves and faeries ally by origin (race field `origin_allies`), wherever they live, never broken by refusal; (3) at a declaration the defender's allies are called, then the attacker's (rarely, only allies hostile to the enemy); an ally that joins opens a `war` row against the enemy and fights in its partner's battles with its levies near the battlefield (new kind `allied_battle` naming both sides' allies); allies take and pay nothing, the principals' peace decides, the allies' dead count in the war score, and a bled-out ally withdraws; calls are made only at a declaration (no chains); a realm in its shelter years is not attacked by allies either (P3's shelter rule kept); (4) a betrayal is a war declared on an ally; allies build strain at a quarter of the rate; the betrayed never re-ally with the betrayer; (5) a bloc is a realm with its direct allies; no alliance forms that puts either bloc above 500 per mille of the non-islet land, and a bloc above 700 sheds its smallest ally at the next step, so P5-00 8.3's 800 per mille / 1,000 years bound holds by construction; the coalition by fear of P3-06 5.1 reads bloc shares; a defender's allies deter declarations (30% of their force in the odds); (6) alliance events move race dispositions only through `RelationsSubsystem::credit` at fixed points with existing events (P5-03a audit A4); (7) trade is a symmetric `relation` row of kind `trade`, opened by a keyed draw in ticks and quiet steps between realms in reach whose disposition is at least −250, broken by war, cooling or loss of reach; its one effect is strength (5% of force per partner, up to three) plus a credit every 50 years; (8) wealth is left out: no rule needs a number; `civilization_census` is not written; (9) vassalage is deferred to a small stage after P5-06 Figures; (10) the tuning id is `history.alliances`, not P5-00 4.6's `economy`; (11) no schema change for parts A and B (relation kinds are text keys, kinds are appended); the report stays `v6` by P3-07 25.1's growth rule (a block `[alliances]` after `[peoples]`, lines appended to civilization blocks; joined `war` rows kept out of every war count so no line changes meaning); browser group `alliances` = 7; (12) the dwarves' mines and arms are a separable part C (site kind 3 `mine`, schema 11), recommended as the opening of P5-07 Mountain depths.
- Interim choice: as proposed. Open for Paul in the spec (24): battles shared or separate wars, spoils, the elf-faerie bond, betrayal and desertion, wealth and vassalage, part C now or in P5-07, the bloc bound by rule.
- Decision (Paul, 2026-11-24), on the spec's seven questions (P5-04 r2 24): (1) allies fight in the partner's battles (`allied_battle`), as recommended; (2) spoils, changed from the recommendation: "the ally should receive a share and if the other ally doesn't want to give it, it might sour the relationship" (r2 6.11: an ally that fought in a won war is owed a share of the tribute and plunder, never land, in proportion to its dead; the principal grants or withholds it by a keyed draw; a withheld share chills the peoples and leaves a grudge between the realms, and two in one term, or one that cools an alliance past breaking, end it as spurned); (3) elves and faeries start allied by origin, wherever they live, never broken by a refusal, faeries seldom sending armies, as recommended; (4) betrayal and desertion as recommended; (5) no wealth number, vassalage later (after figures), as recommended; (6) part C **now** (schema 11, generator 0.26.0), not with P5-07; (7) the bloc bound by rule (500 per mille to form, 700 to keep), as recommended. The lead's rulings of the same day: report format v7 (not v6 by the growth rule), browser group `alliances` = 7 with `ALL_GROUPS` 0xFF, tuning id `history.alliances`, P5-00 3.1 and 6.1 superseded on vassalage, `civilization_census` and `history.economy` (note in P5-00).
- Spec r3 (2026-11-24, spec writer): `p5-04-alliances-trade.md` brought to "as built and ruled", no design change: the lead's ruling A9 (no alliance or trade reopened in the step its row closed), `forgive` at every `alliance_formed` (the spec defect found in calibration), the calibrated values (with `border_friction` 520), the engineer's and the test writer's accepted readings, the mine glyph's cells, two reader messages, and the engineer's measured figures in 15.1 (validation re-measures).
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).
- Engineer's readings (implementation, 2026-11-24, `p5/04-impl`): (1) `ConflictSubsystem::join_war` takes the side code as an argument (`join_war(ctx, war, ally, side, event)`): the spec's form cannot tell the caller's side; (2) in an allied battle a side's dead go to its allies first (ascending id, floor of the force share), then the rest to the principal, the winners' side first (7.7 "in that order"); (3) a share of the spoils is owed only when the war's ending event is a `peace`, `war_won` or `civ_conquered` (the kinds 9.1's reader check accepts); a war ended by a side's `civ_ended` owes nothing; (4) calls to arms (`ally_joined_war`, `ally_refused`) are not summary candidates by themselves, only companions of their war (8.3); (5) alliance and trade lines print `<from>-<to>` with `<to>` the year of the closing event, or E when standing; the basis word `against a common enemy` is followed by ` against c<z>` as 10.2's form gives it; (6) `largest bloc: <bs> permille, ...`: the report text is ASCII (lint rule `non-ascii-literal`), so the per-mille sign is a word; (7) the fear reason reads the bloc shares of the step's start (the cache of 5); the bloc test of a formation reads them live; (8) reach (6.2) is a breadth-first search over the open borders from P(c) to the radius max(`ally_hops`, `trade_hops` + 1), equal to the smallest `PeoplesView::hops` within it; (9) the mine glyph (code 19) is the two diagonals of radius 2 with a head at each upper end, fill (60, 60, 60) with the halo, drawn 7 cells right of and 7 above the site's cell (one glyph cell), between ruins and standing settlements (rows dy = -3 to 3, columns dx = -3 to 3, F fill, H halo, . none: `HHHHHHH`, `HFFHFFH`, `HFFHFFH`, `HHHFHHH`, `HHFHFHH`, `HFHHHFH`, `HHH.HHH`; fill (60, 60, 60), halo the markers' halo (240, 240, 240)); (10) the `pass` value of an allied battle is checked only for not being negative (`HistoryView::load` reads no pass rows); (11) `alliances_years_median` is the lower median; (12) the relation checks of 9.1 are in `HistoryView::load`, which now reads table `relation`, after the checks of P5-03 16.4.
- Engineer's reading found in calibration (defect of the spec, for the lead): two realms that form an alliance kept the strain and claims they held on each other, so almost every betrayal (140 of 140 in the first sweep, 23% of ended alliances, 68 worlds) was a declaration on a claim older than the alliance, and `allied_strain_permille` could not move AL7. Reading taken: at an `alliance_formed` (origin included) conflict drops the pair's strain and claims in both directions, as a peace does (`ConflictSubsystem::forgive`, called from `open_alliance`); a betrayal then comes only from strain built while allied. Lead's ruling A9 is built (a pair whose alliance or trade closed in a step does not open one again in that step), and the test writer's readings A6, A10, A12 and A13.
- Calibration (engineer, 2026-11-24; 1024 defaults seeds 0 to 99, release; `--civs-per-race 4` seeds 0 to 29). Changed from the spec's starting values: `history.alliances` `ally_full_disposition` 300 -> **150** (defence calls were refused more often than answered, AL6), `defend_permille` 700 -> **1000**, `attack_permille` 200 -> **500**, `ally_wars_max` 2 -> **3** (AL5), `spoils_grant_permille` 450 -> **350**, `withheld_memory` 50 -> **100**, `withheld_grudge` 120 -> **400**, `withheld_to_break` 2 -> **1** (SP2: few shares are owed, so one kept share in a term ends the alliance at its renewal), `mine_permille` 20 -> **40**; `history.conflict` `allied_strain_permille` 250 -> **160** (AL7), `ally_deterrence_permille` 300 -> **50**, and `border_friction` 300 -> **520** (C1, the lead's last lever: with alliances and trade the wars fell from 25 to 21.5 at seeds 0 to 49 whatever deterrence, strength or memory did; without alliances and trade, `border_friction` 420 gave 25). Others as specified. Measured (default): formed median 14 (mean 14.4, every world at least 4), standing 9, across races 66%, elves and faeries allied in 100 of 100, wars with an ally 25.8% with 82% of joins in defence, refused 687 against joined 736, betrayal in 50 worlds and 14.5% of ended alliances, ended 525 (lapsed 82, broken 245, spurned 28, betrayed 76, with a realm 94; median length 294 years), bloc share median 368 and max 567 permille, never above 800, allied battles 1,585 of 7,622, shares granted 681 per mille of 119 owed (median 1 a world), spurned in 28 worlds (5.3% of ended), trade opened median 14.5 and standing 10, TR2 43%, a mine in 99 of 100 worlds (median 2 a world, the bottom of MC1's range: one mine per settlement and few dwarven towns of 200 on ore), arms sold in 85 of the 99, wars median 23 (mean 24.4), T16 1.44 per thousand, standing settlements median 83, population 208,364, 53% of turns into friendship, friendship at E median 4, summary median 577 (max 742). `civs4`: summary median 932 (max 1,070), wars median 88, formed median 61, bloc max 633. Timing `generate --seed 0 --map-size 1024`: 17.4 s against 15.5 to 18.9 s for 0.25.0 on the same machine.

### Schema 11: mine sites (P5-04 part C)

- Date: 2026-11-24
- Raised by: spec writer (P5-04 19.6), hard rule 7
- Design doc section: 5 ("Sites": mines; "Dwarves are great forgers"), 6 (`site`)
- Problem: dwarven mines are sites history acts on (P5-00 10.1) and need a site kind code, which is a code list change (P3-04 8.2).
- Proposal: only if part C of P5-04 is taken (in P5-04 or at the opening of P5-07): schema 10 → 11 with the generator of that merge; `site.kind` gains code 3 `mine` (founder civilization set, monster 0; endings 4 and 5 as outposts); no DDL statement changes and the empty world keeps 45 pages; readers refuse 10; event kinds `mine_opened` and `arms_sold` are appended.
- Interim choice: parts A and B only (no schema change); this entry waits for Paul's answer to P5-04 question 6.
- Decision (Paul, 2026-11-24): option C now. Schema 10 → 11 with generator 0.26.0 in the P5-04 merge, as proposed (event kinds `mine_opened` and `arms_sold` are 77 and 78 in P5-04 r2, after the spoils kinds; provisional until the merge order is fixed). P5-07 Mountain depths deepens the same mines and needs no second mine kind (P5-04 r2 6.14).
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).


### History: P5-03a r2, as built and calibrated

- Date: 2026-11-24
- Raised by: spec writer (`docs/specs/p5-03a-relations-followups.md` r2), from the build, the calibration, the lead's rulings and the audit on `p5/03a-int` (generator 0.25.0)
- Design doc section: 5 ("Race relations"; "Deep time"; "Waiting for phase P5"; "Monsters as actors")
- Problem: the readings of the entry above were taken with r1's estimated values and targets; calibration moved some values and some targets were restated against what a default world can give.
- Readings now in force: peace between neighbours as r1 proposed (goodwill after a hundred years without war, 60 a century, up to 300); peoples sealed in by closed borders meet by word within **five** borders after **150** years (r1: three and 300). `history.conflict`'s `border_friction` is **300** (was 200): warmer neighbours fought about three wars a world fewer, and friction, which adds the same drive to every pair in contact, gives them back most where peace took them; it is also the awakening quarrel's limit, which no newly awakened realm's pair reaches. A host named for its place gives a renaming no `{name}` (no more "'s Shadow"). Targets restated (1024 defaults, seeds 0 to 99, measured in brackets): every pair whose lands are joined by any chain of borders meets in at least 95 worlds (99); a race alone on its land mass is reported, not counted (24 pairs in 4 worlds), and no other race meets no one (0 worlds); turns a world at most 20 (16, median 3); at least 25% of turns into friendship (33%); at least 55% of turns before 5500 and 20% before 4500 (57%, 23%); friendship and hatred at E medians at least 3 (3 and 4); no end tally in more than 25 worlds (20); wars median 23 to 28 (25); repeated battles at most 2 per thousand (1.65); standing settlements median at least 81 (85) and population at E at least **205,000** (211,977; r1's 214,500 missed with no defect found: more captures than sacks); expulsions median at most 3, max at most 15 (2, 13); undead risings after 5500 at most 55% (48% of 190); summary lines median 550, max 689, `civs4` median 867.5, max 1,086, within the bounds.
- Interim choice: as built. Left for later: meeting across the sea; `word_since` 0 doubling as "unset" in year 0.
- Status: decided (Paul's answers of 2026-11-24 and the approval of P4). Merged into design.md sections 5 and 12 (2026-11-25).


### Paul's review of P4 Viewer v1 (2026-11-24)

- Section: 13 (phase P4); specs P4-04 section 16, P4-05 section 14, P4-06 section 14.
- Problem: the three viewer specs left eighteen choices open for Paul to judge in the running viewer (marker size by kind, one ring per event place, no markers without history, hidden refuges shown, markers on by default; the browser in a right-hand pane, follow on the filtered list, one level for list and block, highlight by dimming, the glyph changes, a click selects; quiet years at ten times the speed, playback of the browser's list, scrubbing pauses, the ticker over the map, the defaults, the two-row year bar, AVI recording).
- Decision (Paul, 2026-11-24, after using the viewer on seed 42 at generator 0.24.0): "the viewer works great, no concerns with it." The phase is approved and every interim choice stands as built. The lead's lean towards a ticker strip below the map is dropped. Recording on Windows remains untried.


### History depth: the rest of P5 after Paul's scope decision

- Date: 2026-11-24
- Raised by: lead (session of 2026-11-24; `docs/questions-for-paul.md` Q-P5-00-1), recorded by the spec writer with `docs/specs/p5-06-figures.md` r1
- Design doc section: 5 ("The plan for phase P5, history depth": twelve stages), 13 (the P5 row)
- Problem: P5-00 3.1 plans twelve stages; with P5-04's mines in, the lead proposed to keep figures, the pantheon and artifacts, defer roads and the mountain depths, and fold minimal casters into figures and cults into the pantheon.
- Decision (Paul, 2026-11-24): "take the recommendation except for mountain depths". The rest of P5 is: P5-06 Figures with a minimal form of casters folded in (towers and libraries, named books, rare blights; no spells); P5-07 Mountain depths, kept as a small stage on P5-04's mines; P5-08 Pantheon and interventions with cults folded in; P5-11 Artifacts. P5-05 Roads and improved passes go to P6. P5-09's spell invention (the `spell` table, `magic_schools`) and P5-10's factions (noble houses, bandit bands, orders) are deferred. P5-00 3.1 carries a note at each affected stage. The design doc's list of twelve stages (section 5) is out of date on these points; Paul brings it to the design conversation.
- Status: accepted

### History: figures, P5-06 (readings and scope)

- Date: 2026-11-24
- Raised by: spec writer (`docs/specs/p5-06-figures.md` r1)
- Design doc section: 5 ("Entities": figures; "Quiet intervals": ruling lines counted, long-lived figures persist; "Magic in history"; the tick loop item 3; "From history to play": tombs, an heir in exile), 6 (`figure`, `relation`)
- Problem: the design names figures (rulers, generals, heroes, outlaws), their ties and casters without rules or numbers; a default world has about 740 reigns, too many to name.
- Readings taken: (1) every realm has a ruling line of reigns drawn at their start (length about one `generation_years`, accession age and a temper from the race's `temper_weights`), advanced in T3 and, without ticking, in quiet steps (`draw_at`, a keyed draw at the reign's own start year); (2) rulers are named by deed: founders, the ruler under whom the realm declared war, broke faith, granted or kept spoils or fell, the rivals of a disputed succession, and the ruler at the end of the age (about 55 a world); the rest of the line is a count; (3) a succession crisis ends a reign; a split names the winner and the claimant (the new realm's founder), a failed claim may exile the claimant to the friendliest realm in reach; (4) generals named at declarations lead their war's battles (new kind `battle_led`, since a role cannot be added to a shipped battle kind) and may fall; (5) heroes rise after raids, hunt sooner and stronger, and slay (`monster_slain_by_hero`) or are slain (`hero_slain`); (6) minimal casters raise towers (site kind 4), write named books (table `book`) and rarely cause a blight; (7) effects bounded: tempers on strain (normalized per race), spoils and allied strain; a general's skill in the win chance only; a hero in hunts; a short reign in secession; (8) ruler names enter existing sentences (war declarations, betrayals, spoils, falls of realms) by a render form `{r:under}` that reads the `ruled` rows, not by new kinds; (9) epithets earned by deeds; (10) no birth is an event; (11) ties are `ruled`, `served`, `rival`, `parent` relation rows; deeds stay events; (12) report v8 with `[figures]` and a `rulers:` line per realm; browser group `figures` = 8.
- Interim choice: as proposed. Open for Paul in the spec (22): scope C, B or A; naming by deed or every reign of the active phases; regnal numbers; tempers by race; the last ruler of a conquered realm; epithets; births; the summary bound.
- Decision (Paul, 2026-11-24): "Option C, agree with all the recommendations except 3. I think humans and dwarves might have numbers some of the time the rest probably not, 6. If the deeds are substantial it can be more than two per world." So: scope C (minimal casters in this stage); rulers named by deed; tempers by race with each race's average kept; a conquered realm's last ruler falls with it; births are never events; default summary median bound 645. Changed: (3) a new human or dwarven ruler sometimes takes the name of an earlier recorded ruler of the same realm's line and bears a regnal number ("Balin II"; culture field `regnal_permille`, human 250, dwarven 200, the others 0; the first bearer never renumbered; a seceded realm begins its own line; column `figure.regnal`; P5-06 r2 6.14); (6) an epithet entry has an ordinary and a substantial threshold on a measured deed; the cap of two bearers per world holds for ordinary earnings only, a substantial deed gives the epithet however many bear it, held meaningful by the target that no epithet is borne by more than a tenth of a world's figures (P5-06 r2 6.10, FG11). The lead accepted readings (4), (6), (8), `draw_at`, outlaws deferred, report v8, group `figures` = 8 and tower glyph 20 the same day. Spec at r2.
- r3 (2026-11-25, spec writer; for the lead's review, no decision of Paul's reopened): the spec aligned with P5-04 as built at 0.26.0 (event kinds 79 to 92 final; 97 streams; `open_site` for kinds 3 and 4, P5-02's site loops testing kinds 1 and 2; a library's glyph to the upper left of its city, away from the mine's; one library per city; the cause-word kinds 10, 16, 31, 43, 45; figures and alliances stated point by point: rulers named only by `alliance_betrayed` and the two spoils kinds, generals only for a declaration's principals, no alliance rule for disputes or exiles), with Paul's two riders of 2026-11-24 (Q-RUINS-1, Q-P5-01-1): (1) no ruin fades: P3-05 8.6 removed, `ruin_fade_years` and `ghost_fade_years` removed, status 4 and kind 19 kept as defined but never written, the report's faded counts printed only when non-zero, and on the map ruins older than 1,500 years drawn as the faded marker behind the viewer's `Old ruins` switch (the lead's proposal); (2) a major calamity (a famine or plague that killed at least a tenth of its realm's people, by the census before it, or caused a settlement's ruin) is in the summary always, at most 8 a world, ranked first in its phase's calamity cap, inside the summary budget; measured on 0.26.0's stored worlds: 3 to 9 a default world (12 worlds), 21 to 29 a `civs4` world (8 worlds). Recalibration targets with them: a living monster at E in 85 of 100 default worlds, a lair on a former settlement in 65; undead in at least 9 of 30 `low` worlds and 6 of 10 at 2048, not more than half of them in the last 500 years; two undead rules for it (the slaughter threshold no longer raised below normal pressure; the population term scaled by the new `history.monsters.undead_regions_ref`, 64, so a 2048 world needs about what a 1024 world needs per region). T18's default bound 650 (645 for figures plus 5 for the riders).
- Engineer's readings (implementation, 2026-11-25; spec r3):
  1. The four two-word choices of 8.2 (`outcome`, `hosted`, `again`, `library`) are written in the spec as if the first word were for 0; the grammar's two-word choice gives its first word for 1 (P3-04 8.4). Shipped as `{outcome?, who broke away with the far lands|, who was driven out}`, `{hosted?went into exile with |fled into the wilds}`, `{again?again took|took}`, `{library?founded|raised}`, so that the sentences read as the spec's samples (a library is founded, a tower raised).
  2. `history.figures.epithets[].text` accepts an epithet or the slot `{slayer}` alone (4.4 types the text with the grammar `epithet`, which `{slayer}` does not match).
  3. The culture fields of 4.2 are appended inside the `naming` record, after `site_forms` (where `site_forms` is).
  4. In the cause maps of kinds 10, 16 and 31 `battle_led=war` and `blight=blight` follow `allied_battle=war` in that order; kinds 43 and 45 gain `battle_led=the slaughter`.
  5. `FigureView::Hero` gains `fall_permille` (the tuning `hero_fall_permille`, which the monsters' hunt applies, 7.7).
  6. `split_bonus_permille(civ)` (7.4) takes Y as the year of the figures' last step, their T3 of the year, which precedes peoples' T4.
  7. A crisis in the year of an accession whose reign is already named (by its founding, or by a deed read in T3): that ruler is the winner `ruler`; its `ruled` row is not opened again and its start event stays its record.
  8. 6.9's place of death by role: an exile at its host's capital region (its start event's region in the wilds or when the host has ended); a caster at its tower's region; any other figure at its realm's capital region while the realm stands, else at its start event's region; the settlement is the capital only for a ruler of a standing realm.
  9. A general who dies of age while serving leaves his side's slot empty but opens no vacancy (6.6 opens one only for a fall).
  10. L7 (`served` closed) also accepts `search_abandoned`: conflict's `war_end_event` of a war ended unfound (P3-06 r10 20.17).
  11. In Q4 an unnamed reign's next start is its planned end e, guarded below by the step's start Y (never needed: a reign's end lies at or after the step it is read in, unless a named ruler's death waited).
  12. Generals are appointed only in T3 (every `war_declared` is read there, being T2's).
  13. Peoples' Q3 is kept in `steps()` with no action (25.1 (a) leaves it to the engineer).
  14. F26 requires `skill` ≥ 1 while `skill_min` may be 0 (4.4): a tuning with `skill_min` 0 could write a world its readers refuse. Shipped 100; flagged for the spec (raise `skill_min`'s minimum to 1, or F26 to 0).
  15. The readers (report v8, browser, timeline, markers): `succession_disputed` and `exiled` enter the summary only as companions (of their crisis or secession; the exile also of its dispute); `fell_in_battle` is a companion of the war's last battle in the war frame and stays a candidate by itself; `[figures]` deeds count events naming the ruler at the event (`wars declared`, `won` = peaces and wins of its realm plus conquests, `realms conquered`), a general's battles by its general roles; figures of note are those with renown 1 or more; plurals by the existing rule (`1 war declared`, `1 reign`); a realm with no recorded ruler prints `rulers: 0 reigns, 0 recorded`; `rulers_by_deed` counts start events of 6.4 rule 3 only; `figure_age_median` is the lower median; a site with a settlement is a `library`; `figure_at`'s status 2 runs from the year of the figure's `exiled`; an open `served` row ends at E.
  16. Lead rulings G1 to G7 of 2026-11-25 as built (a hero slain in a T4 is no hero for the rest of that T4; L7 takes `search_abandoned`; a general dead of age leaves no vacancy; a skill is at least 1; a ruler named at a realm's end gets `ruled` opened and closed by that event; a split with `name_rulers` false still starts the new realm's reign 1; generals of a war closed by a principal's end stand down with that realm's ending event).
- Calibration (engineer, 2026-11-25; 100 default worlds at 1024, seeds 0 to 99, history replayed on cached geography and checked equal to `generate`): `history.figures` `temper_strain_permille` warlike 2000, peaceable 0 (from 1300, 700; FX2 needs the full spread: 1.41 and 1.44 on the two default sets, 1.24 at 1800/200), `temper_spoils_permille` grasping −400, just 400 (from ±200), `general_scale_permille` 850, `skill_min` 500, `skill_max` 800 (from 100, 300: FX1 49% at the starting values), `fall_loser_permille` 120, `fall_winner_permille` 25, `hero_hunt_permille` 20, `hero_might_permille` 1000; `history.monsters` `living_permille` 200, `overrun_people_ref` 3000, `undead_dead_permille` 28; monsters `rewake_ppm` dragon 700, giant 1000, ogre 2000, troll 1500, wyvern 2500, `lifespan_years` giant 4000, ogre 2000, troll 4000, wyvern 2000, undead 5000, `overrun_permille` 1000 (giant, ogre, troll, wyvern, undead), undead `wake_rate` 25000 and `max_alive` 5. Every other value as the spec's start. Not met: FG5's share of slayings by heroes (27.9% and 27.8%; the hero levers trade against M4a, M4b and M's slain bound, and `hero_scale_permille` is at its top); FG8's figures alive through a quiet step (20%, no lever); FG1's max (226 and 208 against 200, one world each); M4b on seeds 0 to 99 (63 against 65; 68 on 100 to 199). `epithet_substantial_max` not added: FG11 is met by the starting thresholds.
- Status: accepted

### Schema 12: figures and books (P5-06)

- Date: 2026-11-24
- Raised by: spec writer (P5-06 9.4), hard rule 7
- Design doc section: 5 (figures; "Magic in history": books), 6 (`figure`; `spellbook` later)
- Problem: figures need a table (design 6 `figure`), and minimal casters a site kind for towers and a stored name for each book.
- Proposal: schema 11 → 12 with the generator of the P5-06 merge (planned 0.27.0): table `figure` (design 6's columns plus epithet, race, culture, office year, start event, death event, place of death, status, reign number, temper, skill) and, with part C, table `book` (title, author, tower, year, event), appended after schema 11's last statement; `site.kind` gains code 4 `tower` (part C); entity type 6 comes into use; readers refuse version 11; the empty world grows by two pages (to be measured); event kinds 79 to 92 appended; relation kinds `ruled`, `served`, `rival`, `parent` (text keys).
- Interim choice: as proposed, with part C (question 1 of the spec). Without part C, `figure` only.
- Decision (Paul, 2026-11-24): scope C, so schema 12 holds `figure` and `book` and site kind 4 `tower`. With his decision on regnal numbers `figure` also gains the column `regnal` (after `reign_no`: 0 none, else 2 to 39, the bearer's rank among the recorded rulers of its civilization with the same name; P5-06 r2 9.1, 6.14). Everything else as proposed.
- r3 (2026-11-25): the schema text is unchanged; `settlement.status` code 4 `faded` stays valid for readers and fixtures but is no longer written from 0.27.0 (every ruin stays, P5-06 25.1), which `schema.md` notes at the `settlement` columns.
- Status: accepted

### Paul's answers of 2026-11-24 to the open questions

- Section: 13 (phase P5); `docs/questions-for-paul.md`, where each row has his words.
- Problem: 28 questions were open across the specs of P5-01, P5-02, P5-03, P5-03a, P5-04, P5-12 and P4-06, most of them rulings the lead had taken on a recommendation.
- Decision (Paul, 2026-11-24):
  - Scope of the rest of P5 (Q-P5-00-1): "take the recommendation except for mountain depths since we have the mines in this stage already." So: P5-06 figures with minimal casters, P5-07 mountain depths, P5-08 pantheon with cults, P5-11 artifacts; P5-05 roads to P6; P5-09 spell invention and P5-10 factions deferred.
  - P5-04's kept readings (a realm with an open mine is itself stronger; a share of spoils is counted in people; one withheld share can end an already cool alliance): agreed.
  - P5-03a: the three relaxed targets (population 205,000; 16 turns; 23% before 4500) accepted; peace alone can bring friendship, meeting by word, the undead threshold by population, milestones as a count, one expulsion per realm in 60 years, the summary bound by L(C): agreed. On peoples alone on a land mass: "down the road we need to deal with these situations. I think people disconnected like this could thrive and eventually build ships to come to the land but I don't know what stage we want to build that into." (open as Q-SEA-1.)
  - P5-03: realms read disposition, expulsions, flight for fear, the `civs4` bound of 1,150, one stage: agreed. Abandoned ruins: "I'd like these ruins to stay through till the end of simulation since we want to explore those in the later game." (Whether sacked ruins should also stay is open as Q-RUINS-1.) Lairs on former settlements, 62 and 66 worlds against 65: "Sounds fine." Whether relations move enough: "lets see what the critic says" (P5-03a's review).
  - P5-12: about one region in twelve renamed, the wider doubled-suffix rule, English forms only: agreed.
  - Calamities in large-world summaries (P5-01): "We should record these if the impact is significant." The lead takes it into the next stage that changes the report.
  - Weak names (`Thassie`, `fag` anywhere, dwarven `din`): "Leave them in."
  - Movie recording on Windows: not a concern for now; "we can come back and fix it if its broken."
  - Follow-ups (Paul, 2026-11-24, later the same day): "Agree with both proposals, roads and ships in P6." Contact by sea for peoples alone on a land mass is built with roads in one P6 stage; every ruin, sacked ones included, stays to the end of the history (scheduled with P5-06).
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).

### Paul's answers of 2026-11-25

- Section: 13 (phase P5); `docs/questions-for-paul.md` rows Q-P5-03a-M4 and Q-P5-04-8.
- Problem: two questions were left open by the merges of P5-03a and P5-04: the worlds with a living monster at the end and with a lair on a former settlement had fallen under their bounds (85 and 65 of 100), and calibration had made one withheld share of spoils end an alliance, against the reading Paul had agreed to.
- Decision (Paul, 2026-11-25):
  - Monsters and lairs: "Agree, we should recalibrate since the living monsters will be a major highlight of the game when players explore the land." The bounds stay at 85 and 65; P5-06 (spec r3, 25.3) tunes back to them.
  - Spoils: agreed with the recommendation. A first withheld share sours the alliance; a second, or one large kept share, ends it. To be built in the follow-up stage P5-04a, with the world critic's notes on P5-04.
  - The spoils target under that rule (Q-P5-04a-1, later the same day): "I'm good with the way that rule plays out, you can continue." SP2 is restated to a spurning in 10% to 35% of worlds; the P5-04a spec (r1, branch `p5/04a-spec`) is approved.
- Status: decided. Merged into design.md sections 5 and 12 (2026-11-25).


### The balance between the races at the end of history (Paul, 2026-11-25)

- Section: 5 (races), 13 (phases P5 and P6).
- Observation (Paul, in the viewer at 0.26.0 and the 0.27.0 preview): the elves are usually beaten down by the end, often to one region, and leave many ruins. Measured on 100 default worlds at 0.26.0, regions held at the end: goblins median 12 (mean 12.4), humans 3 (5.1), elves 2 (3.3), orcs 2 (3.4), faeries 2 (2.6), dwarves 2 (2.1); no race is ever wiped out.
- Decision (Paul, 2026-11-25): keep it as it is. "It will be interesting to see if dwarven weapons and the roads will make any difference in that."
- For later stages: no target on the spread between races. Validation of the stage that adds roads (P6) and of any change to arms reports this table again, against these figures, so the effect can be seen.
- Status: decided

### History: P5-04a relations and alliances follow-ups (readings)

- Section: 13 (phase P5); spec `docs/specs/p5-04a-relations-alliances-followups.md` r1 (generator 0.28.0, after P5-06; schema stays 12; report v9).
- Problem: the P5-04 and P5-03a reviews left alliances with refusals and betrayals that state no motive, a spurning that can follow a kept share by a century, 538 identical renewal lines in 16 worlds, a thaw worded as a decline, pairs that flip every few centuries, summary milestones that are a cap, and expulsions after a thousand years of subjection. Paul decided the spoils rule on 2026-11-25.
- Readings taken (the spec writer's; for the lead):
  - Spoils (Paul's rule): "large" is a share of at least 600 people or an ally that bore three tenths of its side's dead; a large or second kept share ends the alliance at once, in the same year, so cause and effect read together; a first sours it (memory, grudge, an even chance at the next renewal, a halved chance at the next call). SP2 is restated to 10% to 35% of worlds (question Q-P5-04a-1).
  - Refusals: the defence chance is a product of eight named factors and the smallest names the motive; weariness and a war of its own excuse a refusal (it does not count toward a break); a grasping ruler (P5-06) names the ruler. Attack refusals still write nothing.
  - Renewals: no event unless the pair fought together, shared spoils or changed its basis since; renewals fall on a fixed chain from the formation and `alliance_formed` carries `term`, so the report still counts them.
  - Dwarves' participation restated to 85%: sealed uplands are out of open-border reach; contact across closed borders comes with roads in P6.
  - Expulsions: a people held 800 years or more is expelled only within 150 years of a fresh turn into hatred; the elves' share of expulsions is history (they are the goblins' usual conquest), not a rule.
  - No schema change: value rows on seven kinds and a new code on kind 76's `reason` value, as P5-03a did.
- Status: proposed (r1, for the lead's approval).
### The design doc of 2026-11-25 read against the build (lead)

- Section: 5, 7, 9, 12, 13 (the changes of 2026-11-25: factions between P5 and P6, guilds, townspeople, rulers and their quest lines, style layers and site plans, land marked by history).
- Answer to the open question of section 12, "Rulers in the figures stage": yes. P5-06 (spec r3, generator 0.27.0, in validation) records a table `figure` and `ruled` relations between figures and realms, and at the end of history names the ruler of every living realm (`reigned_at_end`); two 1024 worlds checked (seeds 42 and 2026: 9 of 9 and 6 of 6 living realms have a ruler named at the end). One limit to know: a realm's reigns are counted, but a ruler becomes a recorded figure only by a deed (the founder, a ruler who declares a war, betrays an ally, shares or withholds spoils, is disputed, or reigns at the end), so a realm with 27 to 188 reigns has two or three recorded rulers (target FG2: median 3 to 8 per realm). The present ruler therefore has a founder and a few notable predecessors in the chronicle, not a full line. If quest lines need each ruler's immediate predecessor or family, that is a change to P5-06's naming rule, best taken in the factions stage or a follow-up.
- What the new sections ask of stages not yet specified:
  - Factions stage (after P5-11, before P6): tables for guilds and chapters, founding events citing a figure, spread along `trade` and `alliance` relations; goblin and orc counterparts of the company of arms and the college; thieves and assassins shared. It reads generals and casters of P5-06, so those need enough of both in every world (FG3 generals median 16, FG7 casters about 6 in a default world: enough).
  - P6: site plans (footprint, walls, gates, districts, roads, the ruler's seat as a landmark of the capital) stored in the world file; the four style layers need, per settlement and site, the founding race, the race shares, later holders, how it ended and the years, all of which the world file holds since schema 10.
  - Land marked by history needs a place and a year on every battle, sack, plague, famine, raid and renaming: all present. A battle has a region but no cell; if battlefields are to be placed on the ground, P6 must derive a cell for each (a keyed draw within the region near the goal), which is a derivation, not a change to history.
  - Per-race data for receptions (manner toward outsiders, the width of the individual spread) and the attack-on-sight window are runtime tuning; nothing in the generator.
- Nothing in the design changes of 2026-11-25 conflicts with what is built through 0.26.0 or with P5-06 and P5-04a as specified.
- Decision (Paul, 2026-11-25): keep the naming rule as it is for now. "Maybe the quest writer can create whatever backstory and family history is needed that fits within the history as written." So a ruler's predecessors and family beyond the recorded figures are written by the quest-line writer (design section 9) into the lore file, constrained by the world file: reign counts, the recorded rulers and their years, and the realm's events. The generator records no families.
- Status: decided. Merged into design.md sections 7, 9 and 12 (2026-11-25).

### P5-06 figures as validated: lead rulings on the missed targets (2026-11-25)

- Section: 5 (figures), 12 (living monsters as a highlight of play).
- Problem: validation of generator 0.27.0 (`docs/reviews/p5-06-figures-validation.md`) passed every build and comparison and found no defect in the stage, but measured narrow misses on targets of the spec, on default seeds 0 to 99 and 100 to 199: most figures in one world 226 and 208 (bound 200; medians 77 and 79 inside 60 to 120); monsters slain by heroes 27.9% and 27.8% (30% to 70%); figures alive through a quiet step 20.3% and 20.2% (at least 30%); the larger skill wins 55.7% and 54.3% (55% to 70%); the grasping-betrayal ratio 1.53 and 1.27 (at least 1.5); one world of 200 with the epithet `the Old` on more than a tenth of its figures; worlds with a lair on a former settlement 63 and 68 (Paul's bound 65).
- Ruling (lead): merge as measured. None of the misses makes a world read wrong (world critic: acceptable to merge, `docs/reviews/p5-06-figures-world-critic.md`), and the two targets Paul named are met or within sampling noise (a living monster at the end in 92 and 93 worlds against 85). The follow-up stage P5-04a, which changes world bytes anyway, takes them: raise the thresholds of `the Old` (the spec's 15.2 step 11; no per-world cap on substantial epithets is added, following Paul's "regardless of how many bear it"), retune the heroes' hunt and the quiet-step survival or restate those two bounds from what the worlds show, and take the world critic's F1 (towers never end; a new tower is founded on or beside a standing one) and F5 (the `[figures]` summary block lists only generals).
- Question for Paul (Q-P5-06-1 of the questions file): whether 63 and 68 worlds with a lair on a former settlement meets the bound of 65, stated as an average of two samples. Not blocking.
- Status: ruled by the lead; the lair bound is open with Paul.

### P5-04a spec r2: lead rulings on its questions L-1 to L-8 (2026-11-25)

- Section: 5 (figures, alliances), 12.
- Context: the spec of P5-04a was refreshed against P5-06 as built (r2) and carries P5-06's narrow misses and the world critic's notes. It puts eight questions to the lead and none to Paul.
- Rulings: L-1 a tower ending when its line of casters lapses is deferred (it needs a new ending code); a tower ends with its settlement or when its realm ends, by the existing codes. L-2 no schema version bump for tower endings: no table or column changes, the ending codes exist, and `schema.md` records that a site of kind `tower` may now carry them; the generator version bump covers the reader. L-3 `reign N` is dropped from the summary's ruler lines. L-4 monsters slain by heroes restated to at least 25%. L-5 figures alive through a quiet step restated to at least 15% overall, with 30% for elven and faerie figures as a reported figure only until first measured. L-6 the most figures in one world restated to 250. L-7 and L-8 retune as proposed (skill range 600 to 900; grasping allied strain 3000), within the guard targets.
- Why restate rather than tune (L-4 to L-6): the bounds were estimates set before any world had figures; the worlds read well at the measured values (world critic), and tuning toward them risks the monster targets Paul named, which now pass.
- Status: ruled by the lead.
### The bound on lairs in former settlements is an average of two samples (Paul, 2026-11-25)

- Section: 12 ("Living monsters are a highlight of play").
- Problem: the bound of at least 65 of 100 default worlds with a lair on a former settlement open at the end was measured at 63 (seeds 0 to 99) and 68 (seeds 100 to 199) at generator 0.27.0 (Q-P5-06-1).
- Decision (Paul, 2026-11-25): "Agreed, accept it as the average of the two samples". The bound is: at least 65 on the average of the two samples of 100 default worlds, neither sample below 60. The bound of 85 worlds with a living monster at the end is unchanged (92 and 93 measured).
- Status: decided. P5-04a holds both as guard targets.

### P5-04a as calibrated: lead rulings on the remaining misses (2026-11-26)

- Section: 5.
- Context: calibration (`docs/reviews/p5-04a-calibration.md`) brought the targets inside their bounds on both samples of 100 default worlds, with Paul's two monster bounds held (a living monster at the end in 94 and 93 worlds; a lair on a former settlement in 62 and 68, average 65), except for the items below.
- Rulings: AL7 (betrayals 16.8% and 16.3% of ended alliances against 15%; 23.1% before tuning) is accepted and restated to at most 18%. SP2 as a share of ended alliances (6.6% and 8.4% against 6%) is accepted as it trades against the share of large spoils; Paul's accepted form of SP2 (10% to 35% of worlds) is what validation checks. TW1 (towers ended 7% to 9% against 15%) is restated to at least 5%: the bound was an estimate and the caster-lapse ending is deferred (L-1). FG8 is measured as at 0.27.0 (at least 25 years of a quiet phase inside a life), bound at least 15%. `spoils_large_part_permille` stays at 401 (above the cap of 400, so the people threshold alone decides a large share); the spec records that. RR6 on seeds 100 to 199 (9 worlds against 5) and the turbulent set's refusals above joins are reported by validation and not tuned.
- Status: ruled by the lead.
### How often the deep takes a hold and a realm (Paul, 2026-11-26)

- Section: 5 (the mountain peoples), 12.
- Question (Q-P5-07-1): how often may a whole realm fall to a horror of the deep?
- Decision (Paul, 2026-11-26): "I agree that should be rare 8% sounds right, but an individual city/mine/delving could fall more often than that". A whole dwarven realm ends by the deep in at most 8% of dwarven realms; a single hold falls more often. The P5-07 spec's targets carry both: a hold lost in 15 to 40 of 100 default worlds (DP4), a waking in 25 to 55 (DP2), a realm ended in at most 8% of dwarven realms and at least once in 200 worlds (DP5).
- Status: decided.

### The deep: seals, the horrors, allies and other delvers (Paul, 2026-11-26)

- Section: 5 (the mountain peoples), 9 (quests), 12.
- Decisions (Paul, 2026-11-26, answers to Q-P5-07-2 to 4):
  - A sealed horror: "Agreed the PC may choose to go there and open it or may be told to go into the sealed place by a quest but otherwise it should stay sealed". In generated history a seal holds to the end; at runtime the player may open it by choice or be sent there by a quest. The record (the sealing event, the horror asleep, its lair under the hold) serves both.
  - The three horrors (Ymrach, Ysgarth, Zhakul): "Love it, sounds great". They ship as named.
  - Allies and the deep: "I think they might yes." An ally may answer a deep horror. The spec's r1 said no; r2 adds a rule.
  - Other delvers: "I was thinking that maybe other races that delve, like goblins might also run into these things but less frequently". Goblins delve and may wake a horror, less often than dwarves. The spec's r1 had dwarves only; r2 adds it.
- Work: P5-07's spec goes to r2 with the two additions before integration; the engine and calibration follow.
- Status: decided. For the design doc: section 5 (the deep is not the dwarves' alone; allies may come to a hold's aid against a horror) and section 9 (a sealed horror as a quest place).

### Schema 13: deep monsters (P5-07)

- Section: 5 (the mountain peoples), 12 (living monsters), hard rule 7.
- Problem: the deep horrors of the mountain depths need a `wake` value that the code list of schema 12 lacks.
- Proposal: schema 12 to 13 with generator 0.29.0. `monster_kind.wake` gains 3 `deep`. No table or column changes and the empty world is unchanged; readers refuse 12. A mine's depth is derived from its `delving_deepened` events, not stored in a `site.depth` column. No new `site.ending` or `settlement.ending` code: a fallen hold's mine ends with 4 and a lost hold has settlement ending 2, told apart by its ruin event (`hold_lost`).
- Status: approved by the lead with the spec (r1, 2026-11-26).

### History: P5-07 mountain depths (readings and lead rulings, 2026-11-26)

- Section: 5, 12.
- Spec `docs/specs/p5-07-mountain-depths.md` r1 is approved by the lead; Paul's four questions (Q-P5-07-1 to 4 of the questions file) are not blocking and the build carries the recommendations: a whole realm falls to the deep rarely (at most 8% of dwarven realms, at least one fall in 200 worlds); a seal holds to the end of history and the record is enough for the runtime to let the player break it; the three horrors ship as Ymrach, Ysgarth and Zhakul (a rename is data only while their ids sort after `wyvern`); allies do not answer a deep horror.
- Lead rulings on L-1 to L-7, all as the spec recommends: schema 13 for the one code; deep kinds stand outside the world cap and the one-lair-per-region test at a waking; the forge's depth adds 15 per level to conflict's force, set to 0 if the wars median or the allied-wars share move out of bounds; audit A1 of P5-04a is fixed by a credit that changes memory without a contribution; `settlement.ending` 2 for a lost hold; the `tagged` segment for `allies:` lines is taken if the viewer needs only the new kind; a deep flight without a sack event is accepted and P3-06's invariant 3 and the conflict reference widen.
- Guard: the dwarves' median of regions held at the end (2 at 0.28.0) and standing settlements (82 on seeds 0 to 99 against the bound of 81) must not fall; calibration lowers the fall rates before any other target is traded.
- Status: ruled by the lead.

### A realm's fall to the deep stays rare; 8% is a ceiling (Paul, 2026-11-26)

Status: decided. Q-P5-07-5. As calibrated for P5-07, single holds fall in about 20 of 100 default worlds (at most four in one world), and a whole dwarven realm ended in 1 of 200 worlds (about 0.3% of dwarven realms), against the bound "at most 8% of dwarven realms, at least one fall in 200 worlds". Asked whether 8% was a ceiling or a rate to reach, Paul took the recommendation: "Ok I take the recommendation on the 8%". The build stays as calibrated: a lost realm is a rare legend, lost holds are common. No retune is planned; the lower bound (at least one fall in 200 worlds) stays a target for later recalibrations.

### P5-07 spec r2 and as calibrated: lead rulings L-8 to L-12 and on the misses (2026-11-26)

Status: decided by the lead. Spec r2 (allies' aid at the gate and goblin deep warrens, after Paul's answer to Q-P5-07-4) put questions L-8 to L-12 to the lead; all were approved as the spec recommended (aid is never a war and leaves no event or grudge for a realm that stays away; a warren's depth sits on the settlement by events, with no site, forge or arms; kinds 94 and 96 are reused for warrens; one live warren per realm; goblins wake the deep at a fifth of the dwarves' rate). Rulings at integration: "one live warren" means a realm starts no new warren while it holds a live one (a warren lost and won back may leave two); `delving_permille` is the last field of the race record and absent means 0; the aid's hop count and the warren's age clock stand as built. Accepted misses: the goblins' mean regions (12.82 and 12.86 against 0.28.0's 12.1 and 12.6 plus 0.2; the median is 12 and the warren values do not move it); five holds lost in one world under high monster pressure (bound 4, met on both default samples).

### Schema 14: the pantheon and cults (P5-08)

- Section: 5 (gods, factions, sites), 6 (`god`, `faction`), hard rule 7.
- Problem: the rest of the pantheon needs a domain and a title per god, cults need a table, and shrines, temples, sanctums, champions and prophets need codes that schema 13 lacks.
- Proposal: schema 13 to 14 with generator 0.30.0 (`docs/specs/p5-08-pantheon-cults.md` 9). `god` gains `domain`, `title` and `race` (its statement replaced in place; creators keep ids 1 to n and their names, the nine pantheon gods follow). New table `faction` after `book` (cults are kind 1; the factions stage appends its kinds), with design 6's `civ_id` as `civ` (the founding realm), plus god, founder, seat, start and end years and events, ending and status. `site.kind` gains 5 shrine, 6 temple, 7 sanctum; `site.ending` gains 7 abandoned and 8 suppressed (towers may end with 7); `figure.role` gains 6 champion and 7 prophet; entity type 7 `faction` comes into use; relation kinds `cult_in` and `faith_of`. Readers refuse 13. The empty world gains one root page.
- Status: proposed with the spec (r1, 2026-11-27); the lead approves with the spec.

### History: P5-08 pantheon, interventions and cults (readings, 2026-11-27)

- Section: 2 (scope), 5, 6, 9, 14.
- **Mortal religions and temples.** Design 2's scope table defers "Mortal religions and temples", while the P5 plan takes cults with their sanctums, design 5 lists shrines among sites, design 9 names cult sanctums as a dungeon origin and an NPC schedule has a temple block. Reading: history records cults, temples, shrines and sanctums as entities and places (this stage); a religion as a system of play (priesthoods, services, blessings in play) stays deferred. Proposed wording for design 2: "Deferred: religion in play (priesthoods, services, divine favour during play); history records cults and their temples."
- **Factions across realms.** Design 5 says factions live "inside or across civilizations", design 6 gives `faction` a single `civ_id`. Reading: `civ` is the founding realm; presence in other realms is by relation rows (`cult_in`), as guild chapters may be later.
- **Who the gods watch.** Design 5 says gods "keep acting on the world" and that acts have "causes and consequences" but not why a god acts. Reading: each god keeps a regard for each realm built from deeds its domain cares about (a data table per god, like the renaming rules), never favours a realm of the opposed camp, and acts when the regard is strong; each act cites the deeds that moved it.
- **The five kinds and their effects.** Bless (plenty or valour for some decades), curse (pestilence, withering or defeat), raise a champion (a figure who serves as the realm's hero and general), send a monster (a wild or undead kind with a lair near the realm), mark a site (a shrine: sacred ground makes a settlement thrive and draws a cult's seat; profane ground is shunned by settlers for centuries).
- **Gods in the flare-up rule.** A god whose regard is strong in a quiet age can trigger a flare-up (design 5: "a god acts"), as a monster's waking does; it forces no war.
- **The divine script** stays refused through P5 (design 5); the stage fixes the intervention kinds, the engine's validation and a source/applier split so a script source can replace the seeded one later. The refusal text becomes "divine scripts are not supported yet".
- **Exit check.** Every god camp already takes part in an event (the creators' awakenings); the stage adds a target that a god of each camp acts in at least 80 of 100 default worlds.
- Questions Q-P5-08-1 to 5 (pantheon shape, rate of acts, hidden cults, cults across camps, divine script) and lead questions L-1 to L-12 are in the spec, section 21.
- Status: proposed with the spec (r1).

### P5-08 spec r1: lead rulings on its questions L-1 to L-12 (2026-11-27)

Status: decided by the lead. All twelve are approved as the spec recommends (`docs/specs/p5-08-pantheon-cults.md` 21): one schema bump to 14 for the `god` columns, the table `faction`, the site kinds 5 to 7, the site endings 7 and 8, the figure roles 6 and 7 and entity type 7 (L-1); cults as `faction` rows of kind 1 (L-2); the source and applier split of the interventions (L-3); sent monsters outside the world cap (L-4); the deep's stir in quiet steps, to be dropped if its target cannot be met without breaking DP2 (L-5); one live cult per god (L-6); the tower lapse with `tower_abandoned` and `tower_taken_up` (L-7); aid events written after the outcome (L-8); `renew_written_max` 3 (L-9); event kinds 100 to 110 (L-10); a prophet as founder when no recorded figure fits (L-11); pestilence on the capital's region and acts without disposition effects (L-12). Paul's questions Q-P5-08-1 to 5 are in the questions file; the build follows the recommendations meanwhile.
### The pantheon, its acts and cults: Paul's answers to Q-P5-08-1 to 5 (2026-11-27)

Status: decided. Paul: "I accept the recommendation on all five questions". So: (1) besides the six creator gods the pantheon has nine more, three per camp (good: harvest, oaths, lore; neutral: craft, wild, war; evil: war, death, ruin), each with a title and a name drawn per world; (2) gods act about 10 to 25 times in a default world, 2.5 times as often with `--divine-activity meddling` and under a third as often with `distant`; (3) a cult put down in its last realm may hide at a sanctum in the wild, lasts to the present day and does not re-emerge in history, so the player can find it; (4) a people may take up the cult of a god of the opposed camp, rarely (a tenth of the rate within a camp), and such cults are the ones rulers usually put down; (5) `--divine-script` stays refused in P5; the stage builds the seam a script will plug into. For the design conversation: design 2 defers "mortal religions and temples" while design 5 and 9 have shrines and cult sanctums, and design 6 gives `faction` one `civ_id` while design 5 lets factions span civilizations (readings in the P5-08 spec's proposals entries).

### The 3D heightmap preview starts early, in a second session (Paul, 2026-11-27)

Status: for the design conversation; no design change is needed to start. Design section 13 puts the "3D heightmap preview in the viewer" in P6, after P4 and P5, and section 11 lists it under "Later". Paul starts it now on a second machine, in parallel with P5, in its own Claude Code session (`docs/handoff-3d-terrain-viewer.md`: branches `v3d/*`, only `gdext/`, `game/` and `extern/`, reading the world file). The design may want to record: (1) that the preview is its own track beside P5 and P6 instead of a P6 item; (2) whether it is only a preview inside the viewer or already the fly-across test of section 7 that confirms Terrain3D (milestone 2), which decides how much of chunk upsampling, river carving and streaming its first stage takes on; (3) that Terrain3D is vendored in `extern/` as a second GDExtension, which section 10's layout does not yet show. Paul on (2), 2026-11-27: "Plain preview first": the first stage is a fly-over of the elevation and biome layers; the section 7 test is a later stage.

### P5-08 as calibrated: lead rulings on champions and on the misses (2026-11-27)

Status: decided by the lead. Calibration found that 29 to 31% of divine champions ever fought (target PN5: 50%), with no data lever. Rulings, now in the engine and the spec (6.3, 7.5): a god raises a champion only for a realm with an open war or a monster its hero could hunt; a champion raised in an open war whose side has no general takes command at once, and one whose general lives takes the post when that general dies; a champion hunts ahead of the realm's hero. Measured after recalibration on seeds 0 to 99 and 100 to 199: 71.2% and 72.3% fight. On the misses: CU6 (prophets among cult founders, 61.0% and 70.7%) is restated to at most 70% pooled over the two samples (66%), since a prophet is a fitting founder where a realm has no caster or recorded ruler; DP2 on seeds 100 to 199 (57 worlds with a waking, bound 55) is accepted as an average of the two samples (53.5); the goblins' region median of 14 on seeds 100 to 199 (bound 12; 12 on seeds 0 to 99) goes to validation as a reported guard.

### P5-08 as validated: lead rulings on integration, audit and the world critic (2026-11-28)

Status: decided by the lead. Integration defects: the divine growth factor applies to the growth term before the capacity cap (engine fixed); a creator god's cult spreads by camp, as any god's (the engine stood, spec 6.8 amended, in line with Paul's answer that cross-camp cults are rare); the pantheon has a finishing step in the final year that closes the cults of realms ended that year; a cult founded in a step spreads, is adopted or suppressed only from the next step; both history readers use one problem function. Readings P-R22 to P-R29 of the integration notes stand as built (P-R29: a ruler is not named from a deed once the realm has ended). Audit: A1 (in a quiet step divine growth uses the step's first year) is accepted as the reading of 7.1. Validation: goblin regions median 14 on seeds 100 to 199 against the guard of 12, no realm ended by the deep in 200 worlds against the floor of 1 in 200, and the option-set overshoots are reported and carried, not tuned. World critic: F1 (acts without echo) was checked against the cause table and is narrower than stated (see the lead note in the review); F1 to F10 are follow-ups for the next change that alters world bytes, the marker size (F10) for the viewer.

