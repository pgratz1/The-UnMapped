# The Unmapped

[![ci](https://github.com/pgratz1/The-UnMapped/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/pgratz1/The-UnMapped/actions/workflows/ci.yml)

A single-player, first-person, open-world RPG whose world and history are generated from a seed. Dark high fantasy, exploration first, on flat screen and PC VR. The world generator is a headless C++20 library; the game will be a Godot 4 project whose logic lives in a C++ extension.

**This is an early-stage project.** There is no game yet. What exists today is the foundation of the world generator: a toolkit that writes reproducible world files and proves they are identical on every compiler. See [Status](#status) for exactly where things stand.

![The world viewer playing the history of one human realm of a generated world: territory by year, settlements and ruins, wars and plagues, and the events as sentences](docs/images/viewer-history-movie.gif)

*The world viewer playing a thousand years of one realm's generated history (the Duchy of Brel, seed 42, generator 0.24.0): territory by year, settlements and ruins as markers, and each event as a sentence. Every name, border and war comes from the seed.*

## Contents

- [The idea](#the-idea)
- [Status](#status)
- [Timeline](#timeline)
- [What works today](#what-works-today)
- [Build and run](#build-and-run)
- [How it is built to be reproducible](#how-it-is-built-to-be-reproducible)
- [Repository layout](#repository-layout)
- [How the project is developed](#how-the-project-is-developed)
- [Documents](#documents)
- [License](#license)

## The idea

A new world is generated once, from a seed. The generator builds the geography, then simulates several thousand years of history among the gods and peoples of that world: six races created in a fixed order, their civilizations, wars, migrations, monsters and ruin. Every ruin, dungeon, city, faction and quest in the playable world comes from that simulated history, so every place has a cause the player can discover.

Design pillars, in priority order:

1. **History explains the world.** Every site has a cause in the simulated past.
2. **Exploration is the reward.** Systems exist to make travelling and finding things interesting.
3. **Deterministic from the seed.** The same seed and generator version always give the same world, byte for byte.
4. **Flat screen and VR are equals.** Neither mode is a port of the other.
5. **Generator and game are separate programs.** The generator runs headless and is testable without the engine.

The project is two programs joined by one file:

```
seed + settings ──► worldgen (C++ library + CLI) ──► world.db (SQLite) ──► game (Godot 4 + C++ extension)
                    runs once per world              read-only in play     runs every session
```

The full design is in [`docs/design/design.md`](docs/design/design.md).

## Status

*Last updated: 2026-11-22.*

| | |
| --- | --- |
| **Current phase** | P4 Viewer v1 and P5 History depth, started 2026-11-22 in parallel. Of P4, the stages P4-01 (the timeline in the library: who holds which region in any year, civilization colours, and `render --composite territory [--year Y]`) and P4-02 (the layer catalogue and region names in the library, used by the viewer) are merged; the history view (P4-03) merged on 2026-11-23: the viewer reads a world's history, opens on a territory view at the present, and a year bar (buttons, slider and keys) steps through the years, to the next event or the next change of holder; each realm has a fixed colour by race, borders are black only between different holders, and pointing at the map says who holds the land and since when. The colour follow-ups (P4-03a) merged the same day, from the world critic's review: an olive fourth goblin shade, darker pale shades, borders in free land drawn as a darker line of the land's colour in place of black, and the hover text names a region as it was called in the year shown. Settlement and place markers (P4-04) merged on 2026-11-23: one marker per settlement that exists in the year shown, shaped by kind (hamlet to city, fortress), filled with its owner's colour, with marks for capitals, declining settlements, ruins and passes, and a coloured ring where a battle, siege, sack or plague happened that year; `worldgen-cli render --year Y --markers` draws the same overlay on any view, and in the viewer the markers sit above every view, scale with zoom and say on hover what a place is and how a ruin ended. The browser of civilizations and events (P4-05) merged on 2026-11-23: a right pane lists the civilizations and a filtered list of events (by civilization or place, six kind groups, year range); selecting a civilization shows its block of the history report with names as links and highlights its land, selecting an event jumps to its year, marks its place and shows participants, causes and effects, and with Follow on the year bar steps through the filtered list; names can be searched and a click on the map selects what is under it. The map is drawn as tiles so that a year change redraws in 21 ms where it took 170 ms, markers draw at double size at the fitted zoom, ruins are bolder and plague rings green. The history movie (P4-06) merged on 2026-11-24: Play runs the year bar through the browser's current list of events (a whole history, one civilization's story or only the wars) at 1 to 500 years per second, quiet years pass ten times faster and no year with an event is skipped; a ticker shows each year's sentences and a ring flashes at each event's place; `--play FROM TO --quit-at-end` under Godot's Movie Maker mode records a movie whose frames are the same on every run. With it every stage of P4 is merged; Paul reviewed the running viewer and approved the phase on 2026-11-24. Of P5, the first stage (P5-01, carried-over follow-ups) merged on 2026-11-23 (generator 0.21.0), from the world-critic reviews of the history report and Paul's decisions: a war is fought over the land it was declared for, orcs go to war within a few decades of awakening, grim region names are rarer, a battle's dead are counted once, and the history report (format v2) is shorter and lists open wars and standing claims; a default world has about 25 wars and 680 events. Regions renamed by history (P5-12) merged on 2026-11-23 (generator 0.22.0, schema 8): a region keeps its geographic name until history gives it another, after a conquest, the sack of a capital, a great slaughter or a plague ("After the slaughter, the Grove of Wrecks became known as the Red Wood"); about four regions in a default world are renamed, the report and the timeline use the name in force at the time, doubled human name endings are gone, and the name filter also applies where the parts of a region name join. Beasts and monsters (P5-02) merged on 2026-11-23 (generator 0.23.0, schema 9): five kinds of beast live in the regions that suit them, and six kinds of monster (dragon, ogre, undead host and others) wake, take a lair, raid the settlements around it, preferring small ones, and are hunted, slain or die out; a monster can overrun a settlement and lair in its ruin, and settlements now also end by plague, famine or being left, so a default world has about three overrun places and three ghost towns to explore, about eight monsters and twelve watchtower outposts; a ravaged region can take a name from its monster, and the history report (format v4) lists monsters and sites. Race relations, mixed settlements and migrations (P5-03) merged on 2026-11-24 (generator 0.24.0, schema 10): each pair of races that has met holds a disposition (hatred, wariness or friendship) that starts from the races' natures and moves with what their realms do to each other (wars, conquests, sackings, shelter given), and realms weigh it when they choose a quarrel; a settlement records the peoples living in it, so refugees and conquest make mixed towns (about eight in a default world), and a hated minority can be expelled; settlers flee a place a monster keeps raiding, bold beasts trouble the villages near them, a monster's raids follow on from its earlier ones, and a ruin left by flight, plague or famine stays on the map; the history report (format v5) has a `[peoples]` block. P3 History core, started 2026-11-19 and reviewed and approved by Paul on 2026-11-22: the overview and the first three stage specs are approved. History data and settings (P3-01) merged on 2026-11-19 (generator 0.12.0): the six races, their cultures and the history tuning are data files, and `generate` accepts the world-creation settings (history span, detail, races present and others), which do not change a world yet. Names (P3-02) merged the same day (no world change): a library that builds names for gods, civilizations and settlements from each culture's syllables, unique across a world, and the keyed random draws every history decision uses. Chronology (P3-03) merged on 2026-11-19 (generator 0.13.0, schema 6): a world now records which races are present, the year each awakens, the ages between, and a named creator god for each race. All seven stage specs are approved. The simulation engine (P3-04) merged on 2026-11-19 (generator 0.14.0, schema 7): the history stage divides the span into simulated phases and quiet intervals and writes an event log with causes, civilizations, settlements and region control; Peoples (P3-05) merged on 2026-11-20 (generator 0.15.0): each race founds civilizations in the lands that suit it, settlements grow from hamlets to cities, peoples colonise within reach of their capitals, famine and plague strike in active phases, realms split by secession and failing settlements are abandoned; a default world has about ten civilizations, ninety settlements and 550 events. Expansion and conflict (P3-06) merged on 2026-11-20 (generator 0.16.0): civilizations claim neighbouring land and go to war over it, with battles, sieges, sacked and captured settlements, truces and ceded regions; a default world has about 24 wars and 820 events, and most ruins are now made by war. Names and peoples follow-ups (P3-08) merged on 2026-11-20 (generator 0.17.0), from the world-critic reviews and Paul's decisions: regions have names without numerals drawn from word lists ("the Wood of Stags"), generated names pass a filter, faeries live in forest, dwarves found fortresses on high, ore-rich ground, and the event log is quieter (a default world has about 650 events). Conflict balance (P3-09) merged on 2026-11-20 (generator 0.18.0), from the world-critic review of conflict and Paul's decisions: a people's last refuge can flee into hiding instead of being destroyed, a lost war costs the loser land or tribute, the winner of the last battle never loses the war, faeries are hard to find rather than strong in battle, and no race goes extinct in 375 test worlds; a default world has about 23 wars and 730 events. Wording, site and conflict follow-ups (P3-10) merged on 2026-11-21 (generator 0.19.0), from two world-critic reviews and Paul's decisions: the log names plague outbreaks and quiet stretches of at least fifty years and says why wars and migrations happen, each land type has one plain compass name, faeries live inland and dwarves on high ground, an attacker wins a war only by holding its goal with no defender left standing in it, orcs awaken with a quarrel and stay hidden for a shorter time, and the long test suites now run in continuous integration; a default world has about 23 wars and 680 events. Orc arrival, wording and name follow-ups (P3-11) merged on 2026-11-22 (generator 0.20.0), from the second world-critic review of P3-10 and Paul's decision on faerie names: orcs awaken in numbers and send out their first colony at once, so they act within about a decade of awakening where they used to wait two centuries, and the last age is the most violent; claims and ceded footholds name the towns at stake; faerie names lose their `p` and `b` sounds, no name in any culture repeats a syllable twice in a row, and counts from one to nine are written as words; a default world has about 25 wars and 680 events. The history report (P3-07) merged on 2026-11-22 (no world change): `worldgen-cli report --history` prints a world's history as text, at a summary level of the most notable events or in full; its text is identical on every compiler and platform, like the world itself. P0 Foundation, P1 Geography (reviewed and approved by Paul on 2026-11-19) and P2 Godot bring-up and viewer v0 are complete |
| **Next phase** | P6 Derivation and polish, after P4 and P5 |
| **Current milestone** | Milestone 0, Geography generator: met and approved by Paul on 2026-11-19. Now working towards milestone 1, History generator |
| **Work in flight** | P1: elevation and hydrology are merged, so `generate` writes a continent with ranges, passes, rivers and lakes. Elevation followed five rounds of design exploration and two world-critic reviews (`docs/reviews/`): irregular continents, long segmented ranges with 2,000 to 2,500 m peaks, valleys cut by erosion, and a guaranteed share of mountains in every world. Generator 0.6.0 (2026-11-17) added natural passes at saddles of each range's drainage divide, after five more exploration rounds and three critic reviews: a pass is cut by at most 500 m, so naturally low saddles become low gates and others stay high cols, and about two in five are walkable end to end at the default map size. Pass sites are stored in a `pass` table for the history simulation (the first schema change, version 2). Very steep faces are bounded by a final slope-relaxing step. Climate merged the same day (generator 0.7.0): temperature falls with height and from south to north, and each world has one prevailing wind that makes ranges wet on the windward side and dry in the lee. Biomes merged on 2026-11-18 (generator 0.8.0, schema 3): alpine, highland moor, marsh, forest and grassland by rules held as data, with forest thresholds scaled to map size; the biome definitions and the world's wind direction are stored in the world file. A variety revision merged the same day (generator 0.9.0, schema 4; Paul's decisions of 2026-11-17): heath, steppe, wet woodland along large rivers and fens on wet lowland bring the count to nine natural biomes, each world draws its own temperature offset of up to 2 °C either way (stored in the world file), and the coastal forest ring is thinner. Validated on 271 worlds; a world-critic review (`docs/reviews/p1-04b-biome-variety-critic.md`) notes two follow-ups, a visible heath stripe on some worlds and blocky fen patches on the largest maps. Resources and habitability merged on 2026-11-18 (generator 0.10.0, schema still 4): six score layers (fertility, ore, timber, fresh water, forage and habitability, each 0 to 255 and absolute, so a harsh world reads as harsh) and a layer of walkable areas, for the history simulation to place homelands, farms, mines and settlements. Habitability weighs farmland most, then wild food, timber and ore, and treats fresh water as a necessity. Validated on 271 worlds; a world-critic review (`docs/reviews/p1-05-resources-critic.md`) led to a tuning revision before the merge and records the remaining limits. Regions, the last geography stage, merged on 2026-11-19 (generator 0.11.0, schema 5): land is divided into named regions of about 2 km² each, roughly 13, 50 and 200 per map, with borders on ridge crests and large rivers, upland regions of their own for wide massifs, and placeholder names such as "Northwest Steppe II" until history renames them. Three new tables give the history simulation each region's summary (area, biomes, mean resource scores, walkable ground, reachable ore) and each border's length, lowest crossing, river and pass. A world-critic review (`docs/reviews/p1-06-regions-critic.md`) led to a revision before the merge (each pass belongs to one border; mountain and lake regions get fitting names). Validated on 271 worlds (`docs/reviews/p1-06-regions-validation.md`): a default world takes about 9 seconds and 6.5 MB. The viewer gained a regions view, a legend and a description of the cell under the mouse. Paul reviewed and approved the geography phase on 2026-11-19 and accepted the world critic's recommendations, among them the default map size (1024) as the reference size for history. P3 History core started the same day with its overview spec: the shared model of years, ages, peoples and the event log that the history stages build on. |
| **`main`** | Builds and passes all tests on Linux (GCC, Clang, Clang with sanitizers) and Windows (MSVC), in debug and release |

**What "P0 done" means.** The phase's exit check is that an empty world file is byte-identical across all three compilers and that `worldgen-cli hash` proves it in continuous integration. It does: eight test worlds, six empty and two holding test layers, are identical across all seven build configurations, and each matches its recorded golden hash. CI repeats the comparison on every push. With geography stages merged the comparison covers twelve worlds, four of them full maps with elevation and hydrology at 512 and 1024.

**Since P0 was first completed**

- Paul reviewed P0 and merged its fourteen proposals into the design document.
- Seed phrases are now scrubbed, a design change made at that review. This changed the bytes of phrase worlds, so the generator version went from 0.1.0 to 0.2.0 and all golden hashes were re-recorded: the first use of the version rule.

**Known gaps and notes**

- Geography is complete as planned: elevation, hydrology, climate, biomes, resources and regions. History does not exist yet.
- Region limitations (`docs/reviews/p1-06-regions-critic.md`): lowland borders are plain distance borders, not natural lines; some large rivers run inside a region; placeholder names repeat heavily on the largest maps (up to "Northeast Wood XXIX"); islets of any size are regions.
- Resource score limitations (`docs/reviews/p1-05-resources-critic.md`): the banks of large rivers score slightly below their hinterland, lowland areas differ little from each other, and rich ore is often on crags that cannot be reached on foot. Small maps (512) are mostly mountain and leave little room for homelands.
- Biome limitations (`docs/reviews/p1-04-biomes-critic.md`), being addressed by the revision in progress: every range has the same ring of moor on its wet and dry sides; the first kilometre of most coasts is forest; marsh is thin ribbons; rivers through grassland have bare banks.
- At the largest map size about one world in twenty has very little high ground or a peak above the intended maximum (`docs/reviews/p1-04-biomes-validation.md`, defects D1 and D2); to be fixed in elevation.
- Climate limitations (`docs/reviews/p1-03-climate-critic.md`): rainfall depends on map size (small maps are wetter); every world has the same temperature range; the wind direction is not stored yet; passes do not channel moisture.
- Known elevation limitations are listed in section 11 of the elevation spec and in `docs/reviews/p1-01b-elevation-passes-critic.md`. The main ones: low gates on small maps can cut a range down nearly to the plain; a pass sometimes erases a mountain lake on its crossing; about two in five small (512) worlds get no pass; on a rare small map a range drops into the sea as a very steep smooth face (512, seed 41); filled basins among peaks can look smooth; a few worlds get ring-shaped ranges. Rivers on gently sloping lowland run in angular steps (`docs/reviews/p1-02-hydrology-critic.md`).
- The only registered data kind is generator tuning (`game/data/worldgen/`); biomes, races and the rest come with their phases.
- The noise primitive's output stays within about two thirds of its nominal range, which is normal for this kind of noise; the P1 elevation spec needs to account for it.
- The `render` image-size limit is tested only on the "too large" side.
- The sanitizer build's test suite takes about four minutes, mostly one noise range test.
- Agent-written commits before 2026-11-15 credit "Claude Fable 5.2" although Opus subagents wrote them; later commits credit the model that wrote them.

## Timeline

The first block of work is the world generator prototype: seven phases, P0 to P6, that together deliver milestones 0 and 1 and a viewer. Game milestones 2 to 10 follow. Phases marked "parallel" can run at the same time once their dependencies are done.

### World generator prototype

| Phase | Delivers | Depends on | Status |
| --- | --- | --- | --- |
| **P0 Foundation** | Repository layout, CMake presets, vendored dependencies, CI on three compilers, the deterministic core, the reproducible SQLite layer, the TOML loader, `worldgen-cli` | — | **Done** 2026-11-15, reviewed |
| **P1 Geography** | Elevation, erosion, hydrology, climate, biomes, resources and regions, each writing its layer; image export per layer | P0 | **In progress** since 2026-11-15: first three specs approved, shared foundation, elevation and hydrology merged (2026-11-16); natural passes merged (2026-11-17, generator 0.6.0, schema 2); climate merged (2026-11-17, generator 0.7.0); biomes merged (2026-11-18, generator 0.8.0, schema 3); biome variety revision merged (2026-11-18, generator 0.9.0, schema 4); resources and habitability merged (2026-11-18, generator 0.10.0); regions merged (2026-11-19, generator 0.11.0, schema 5). **Done** 2026-11-19, reviewed and approved by Paul |
| **P2 Godot bring-up and viewer v0** | Godot version pinned, C++ extension building on Linux and Windows, `world.db` reader bound into Godot, 2D map with switchable layers | P0; parallel with P1 | **Done** 2026-11-15, awaiting Paul's hands-on review. Godot 4.7.2 and godot-cpp 10.0.0 pinned; the extension builds and its headless check passes on Linux and Windows |
| **P3 History core** | Race and culture definitions, ages and the awakening schedule, active phases and quiet intervals, civilizations, settlements, wars, ruin, the event log, a text history report | P1 | **In progress** since 2026-11-19: overview and all seven stage specs approved; history data and settings (generator 0.12.0), names, and chronology (generator 0.13.0, schema 6) merged (2026-11-19); simulation engine (generator 0.14.0, schema 7) merged (2026-11-19); peoples (generator 0.15.0) merged (2026-11-20); conflict (generator 0.16.0) and the names and peoples follow-ups (generator 0.17.0) merged (2026-11-20); conflict balance (generator 0.18.0) merged (2026-11-20); wording, site and conflict follow-ups (generator 0.19.0) merged (2026-11-21); orc arrival, wording and name follow-ups (generator 0.20.0) merged (2026-11-22); the history report merged (2026-11-22). **Done** 2026-11-22, reviewed and approved by Paul |
| **P4 Viewer v1** | Timeline scrubber, territory by year, settlement and site markers, browser for civilizations and events | P2, P3; parallel with P5 | **Done** 2026-11-24, reviewed and approved by Paul: the timeline, layer catalogue, history view, markers, browser of civilizations and events, and the history movie (P4-01 to P4-06) |
| **P5 History depth** | God interventions, beast populations, named monsters, figures and factions, race relations, casters and spellbooks | P3 | **In progress** since 2026-11-22: P5-01 (generator 0.21.0) and P5-12 regions renamed by history (generator 0.22.0, schema 8) merged (2026-11-23); P5-02 beasts and monsters (generator 0.23.0, schema 9) merged (2026-11-23); P5-03 race relations, mixed settlements and migrations (generator 0.24.0, schema 10) merged (2026-11-24) |
| **P6 Derivation and polish** | Sites, dungeon records, quest hooks and spellbook placement derived from history; world-creation settings on the command line; 3D heightmap preview | P4, P5 | Not started |

```
P0 ██████████ done ──┬─► P1 Geography ██ done ──► P3 History core ──┬─► P5 History depth ──┬─► P6 Derivation
                     │                              (you are here)         │                      │
                     └─► P2 Viewer v0 ██ done ─────────────────────────────┴─► P4 Viewer v1 ──────┘
```

### Milestones

| # | Milestone | Reached when | Status |
| --- | --- | --- | --- |
| 0 | Geography generator | `worldgen-cli` writes a world with all geography layers; maps look plausible in image export and in the viewer; identical file hash on Linux and Windows | **Met**, approved by Paul 2026-11-19: all geography layers are written (P1), the viewer shows them (P2), and the file hash is identical across GCC, Clang and MSVC on Linux and Windows |
| 1 | History generator | Default history generates in under two minutes; races appear in order; the event log reads as coherent history; sites and quest hooks derived | In progress (P3 started 2026-11-19; P3 to P6) |
| 2 | Walkable terrain | Godot streams terrain from a world file; the player can cross the map on flat screen without hitches | Not started |
| 3 | VR parity | The same build starts in VR or flat; the fly-across test holds 90 Hz in VR | Not started |
| 4 | Biomes and wilderness | Six biomes with generated materials, rocks and vegetation; rivers, lakes and roads | Not started |
| 5 | Settlements | A generated city and villages with enterable interiors furnished with physics objects | Not started |
| 6 | Dungeons | Graph-based dungeons for two origin types, full of physics objects, in both modes | Not started |
| 7 | Core play | Combat, book-learned spells, skill progression, inventory, dialogue and saving in both modes | Not started |
| 8 | History in play | Quests from hooks, books and dialogue rendered from real history, local-LLM dialogue. First playable version | Not started |
| 9 | Voice dialogue | Speaking to NPCs and hearing replies, all on local models | Not started |
| 10 | Spell research | Composing, researching and casting new spells within the rules | Not started |

Milestones 0 and 1 carry the most design risk, since they decide whether generated history is interesting. Milestones 2 and 3 carry the most technical risk.

### What P0 delivered

| Stage | Spec | What it is |
| --- | --- | --- |
| Deterministic core | [`p0-01`](docs/specs/p0-01-deterministic-core.md) | One fixed-point number type (32 integer and 32 fractional bits), the project random generator (xoshiro256\*\*), seed streams derived by hashing the seed with a stage name or coordinates, integer gradient noise, integer square roots, SHA-256 |
| World database | [`p0-02`](docs/specs/p0-02-world-db.md) | A reproducible SQLite file: pinned build options, a `meta` and a `layer` table, a compact layer codec, a writer that makes the bytes independent of write order, and a read-only reader |
| TOML loader | [`p0-03`](docs/specs/p0-03-toml-loader.md) | Settings file, data-file discovery and validation in a fixed order, a content hash, and no floating point on the path to the generator |
| `worldgen-cli` | [`p0-04`](docs/specs/p0-04-worldgen-cli.md) | `generate`, `render`, `report` and `hash` |

Around them: about 300 automated test cases, 2,201 golden vectors for the core produced by an independent Python reference, a second independent Python check of world-file contents, a lint for constructs that break determinism, and a CI job that compares world hashes across every compiler.

## What works today

`worldgen-cli` writes world files and inspects them. A generated world currently holds a table of mountain passes and a table of biome definitions, the world's wind direction and temperature offset, and eleven geography layers (elevation; water, water depth, flow direction, flow accumulation, rivers, lakes and distance to the coast from hydrology; temperature and rainfall from climate; biomes) on a 512, 1024 or 2048 cell map at 16 m per cell. Since generator 0.13.0 it also holds the first history tables: the races present, the year each awakens, the ages between, and a named creator god for each race. Since 0.14.0 it also holds a history: phases, an event log with causes, civilizations, settlements with their populations, and which civilization holds each region; since 0.15.0 that history is of peoples growing, colonising, suffering famine and plague, and splitting; since 0.16.0 they also press claims on each other's land and fight wars, with battles, sieges, sacked and captured settlements, truces and ceded regions; since 0.18.0 wars end with a winner, tribute or a truce, and a people driven from its last home can flee into hiding; since 0.19.0 the log also records plague outbreaks and long quiet stretches, and gives the cause of wars and migrations; since 0.20.0 the orcs act from their first years, and claims and cessions of single towns name them; since 0.21.0 a war is fought over the land it was declared for; since 0.22.0 (schema 8) history can rename a region, and the table `region_name` records each name with the year it began; since 0.23.0 (schema 9) beasts and named monsters live in it, with their lairs and raids; since 0.24.0 (schema 10) the races hold dispositions toward each other and settlements record the peoples living in them. `report --history` prints that history as a readable text.

```
$ worldgen-cli generate --seed-phrase "The Unmapped"
/home/you/.cache/the-unmapped/out/world-15629206466951235176.db

$ worldgen-cli report --db ~/.cache/the-unmapped/out/world-15629206466951235176.db
# worldgen report v2
[meta]
content_hash = 8e914ed2ab95b211b2fa87ad47d67ece6ad5a4f38c20e1d2e3ff9d8ff44b736a
format = the-unmapped-world
generator_version = 0.24.0
history_end_year = 6000
param.civs_per_race = auto
param.divine_activity = normal
param.divine_script =
param.event_intensity = normal
param.history_detail = normal
param.history_span = 6000
param.map_size = 1024
param.monster_pressure = normal
param.races = elf,goblin,dwarf,faerie,human,orc
schema_version = 10
seed = 15629206466951235176
seed_phrase = the unmapped
stages = elevation,hydrology,climate,biomes,resources,regions,chronology,history
[tables]
age = 6
beast = 5
beast_population = 613
biome = 12
civilization = 11
climate = 1
culture = 6
event = 678
event_cause = 456
event_kind = 62
event_kind_role = 126
event_kind_value = 59
event_participant = 1454
event_value = 1024
god = 6
layer = 19
meta = 17
monster = 13
monster_kind = 6
pass = 3
phase = 20
race = 6
race_biome = 48
race_relation = 31
region = 51
region_biome = 255
region_border = 126
region_control = 45
region_name = 3
relation = 44
settlement = 98
settlement_census = 1139
settlement_race = 126
site = 26
[layers]
elevation i16 1024x1024 codec=1 bytes=1175180 min=-2306 max=19803
water u8 1024x1024 codec=1 bytes=34948 min=0 max=3
water_depth u16 1024x1024 codec=1 bytes=640156 min=0 max=2306
flow_dir u8 1024x1024 codec=1 bytes=236314 min=0 max=8
flow_accum u32 1024x1024 codec=1 bytes=468500 min=0 max=120835
river u8 1024x1024 codec=1 bytes=27457 min=0 max=8
lake u32 1024x1024 codec=1 bytes=2234 min=0 max=4
coast_dist i16 1024x1024 codec=1 bytes=1022256 min=-6579 max=5008
temperature i16 1024x1024 codec=1 bytes=274872 min=-65 max=84
rainfall u16 1024x1024 codec=1 bytes=1019690 min=497 max=4535
biome u8 1024x1024 codec=1 bytes=48513 min=1 max=12
fertility u8 1024x1024 codec=1 bytes=164027 min=0 max=255
ore u8 1024x1024 codec=1 bytes=185325 min=0 max=255
timber u8 1024x1024 codec=1 bytes=113153 min=0 max=255
fresh_water u8 1024x1024 codec=1 bytes=360860 min=0 max=255
forage u8 1024x1024 codec=1 bytes=196390 min=0 max=255
habitability u8 1024x1024 codec=1 bytes=255314 min=0 max=199
walk_area u16 1024x1024 codec=1 bytes=24902 min=0 max=2
region u16 1024x1024 codec=1 bytes=23636 min=0 max=51
[geography]
cells = 1048576
land_cells = 419430
ocean_cells = 629146
land_fraction = 0.4000
landmasses = 1
largest_landmass_share = 1.0000
coastline_m = 60320
coast_ratio = 1.6421
land_solidity = 0.8933
elevation_m = 0.1 4.3 41.8 64.2 114.9 538.2 941.0 1531.7 1980.3
depth_m = 2.9 140.8 230.6
land_above_m = 50:0.8351 500:0.2686 1000:0.0842 1500:0.0119 2000:0.0000
max_step_m = 77.2
step_p999_m = 60.8
slope_over = 35:0.3103 45:0.2163
slope_p50_above_500m = 1.1798
lowland_flat_share = 0.0162
edge_parallel_run_m = 2288
passes = 3
natural_gaps = 0
passes_capped = 1
pass_col_m = 257.8 316.7 579.4
passes_walkable = 2
lakes = 4
lake_cells = 12438
lake_share_of_land = 0.0297
largest_lake_cells = 7820
river_cells = 8370
river_share_of_land = 0.0200
river_sources = 208
river_mouths = 49
major_rivers = 15
longest_river_m = 11685
largest_basin_share = 0.2881
wind_toward = NW
temp_offset_c = -1.0
temperature_c = -6.5 -0.7 4.2 6.1 7.7
rainfall_mm = 497 553 706 1457 4535
land_below_0c = 0.1249
rain_contrast = 2.6347
biome_share = temperate_forest:0.2927 grassland:0.1449 marsh:0.0053 highland_moor:0.1038 alpine:0.1533 blighted_waste:0.0000 heath:0.0798 steppe:0.1646 wet_woodland:0.0182 fen:0.0374
biome_patches = 475
scores_p50 = fertility:72 ore:0 timber:26 fresh_water:139 forage:89 habitability:40
scores_p90 = fertility:255 ore:81 timber:255 fresh_water:235 forage:180 habitability:142
fertile_share = 0.3921
habitable_share = 0.4303
ore_rich_share = 0.0668
walk_areas = 2
walk_main_share = 0.6155
walk_share = 0.6259
regions = 51
region_area_km2 = 0.82 1.78 5.25
region_borders = 126
region_borders_walkable = 95
region_borders_river = 19
region_borders_pass = 3
regions_isolated = 0
regions_upland = 9
[chronology]
races_present = 6
awakening_years = 0 2094 3327 4226 5036 5638
age_years = 2094 1233 899 810 602 362
age_years_min = 362
gods = 6
[history]
end_year = 6000
phases_opening = 6
phases_quiet = 8
phases_flare = 5
phases_final = 1
simulated_years = 1322
simulated_permille = 220
events = 678
events_awakening = 6
events_civ_founded = 8
events_settlement_founded = 8
events_quiet_era = 0
events_flare_up = 5
events_colony_founded = 90
events_became_village = 20
events_became_town = 58
events_became_city = 3
events_settlement_dwindled = 4
events_famine = 44
events_plague = 0
events_plague_spread = 57
events_succession_crisis = 12
events_civ_seceded = 3
events_settlement_abandoned = 0
events_capital_moved = 2
events_civ_ended = 0
events_ruin_faded = 3
events_claim = 26
events_war_declared = 13
events_battle = 46
events_battle_at_pass = 0
events_siege = 24
events_sacked = 6
events_settlement_captured = 3
events_peace = 3
events_truce = 3
events_region_ceded = 4
events_civ_conquered = 0
events_settlement_abandoned_to = 2
events_refugees_settled = 0
events_tribute = 6
events_war_won = 7
events_search_abandoned = 0
events_refuge_held = 0
events_plague_outbreak = 80
events_quiet_stretch = 8
events_claim_settlements = 0
events_settlements_ceded = 0
events_claim_dropped = 0
events_refuge_hidden = 0
events_region_renamed = 3
events_monster_appeared = 9
events_undead_rose = 4
events_monster_woke = 3
events_monster_slept = 11
events_monster_raid = 40
events_settlement_overrun = 3
events_monster_slain = 5
events_monster_driven_off = 0
events_monster_died = 5
events_beasts_bold = 2
events_outpost_built = 10
events_outpost_abandoned = 5
events_settlement_fled = 1
events_peoples_met = 15
events_disposition_turned = 0
events_refugees_arrived = 3
events_settlement_mixed = 5
events_standing_changed = 0
events_people_expelled = 0
civilizations = 11
civilizations_alive = 11
settlements = 98
settlements_standing = 87
regions_held = 30
census_rows = 1139
relations = 44
region_names = 3
beasts = 5
monster_kinds = 6
beast_populations = 613
monsters = 13
sites = 26
race_relations = 31
settlement_races = 126
population = 217854
population_elf = 7960
population_goblin = 112058
population_dwarf = 10584
population_faerie = 4848
population_human = 72412
population_orc = 9992
settlements_standing_elf = 5
settlements_standing_goblin = 49
settlements_standing_dwarf = 4
settlements_standing_faerie = 10
settlements_standing_human = 14
settlements_standing_orc = 5
regions_held_elf = 2
regions_held_goblin = 16
regions_held_dwarf = 1
regions_held_faerie = 4
regions_held_human = 5
regions_held_orc = 2

$ worldgen-cli hash --db ~/.cache/the-unmapped/out/world-15629206466951235176.db
38f412c511be443691ca2db745f8e59a3106d409b179f7706a882459b76790bc

$ worldgen-cli render --db ~/.cache/the-unmapped/out/world-15629206466951235176.db --composite map
/home/you/.cache/the-unmapped/out/world-15629206466951235176.map.png
```

The history of the same world, as a readable report (`--level full` lists every event; the excerpt is cut at each `...`):

```
$ worldgen-cli report --db ~/.cache/the-unmapped/out/world-15629206466951235176.db --history
# worldgen history v5
level = summary
...
[ages]
a1 The Age of Elves: 0-2093, 2094 years; Elves (ra1) made by Esis (g1, good); 1 civilization founded: 1 at the awakening, 0 by secession
a2 The Age of Goblins: 2094-3326, 1233 years; Goblins (ra2) made by Slatbeex (g2, evil); 2 civilizations founded: 2 at the awakening, 0 by secession
a3 The Age of Dwarves: 3327-4225, 899 years; Dwarves (ra3) made by Nukgarin (g3, neutral); 1 civilization founded: 1 at the awakening, 0 by secession
a4 The Age of Faeries: 4226-5035, 810 years; Faeries (ra4) made by Sofosh (g4, good); 2 civilizations founded: 2 at the awakening, 0 by secession
a5 The Age of Humans: 5036-5637, 602 years; Humans (ra5) made by Birdsten (g5, neutral); 1 civilization founded: 1 at the awakening, 0 by secession
a6 The Age of Orcs: 5638-5999, 362 years; Orcs (ra6) made by Uul'duk (g6, evil); 4 civilizations founded: 1 at the awakening, 3 by secession
[chronicle]
threshold = 20
events = 678
shown = 231
== a1 The Age of Elves: 0-2093
-- p1 opening: 0-149, 150 years; trigger e1; 7 events
...
[civilizations]
c1 The Realm of Lecier
  race: Elf (ra1)
  home: Stagbarrow Tangle (rg34), temperate_forest
  founded: 0 in The Age of Elves (a1), e2
  capital: Vaenies (s1)
  peak regions: 2, 1675-2598
  peak population: 8793 in 5637
  settlements: 8 founded, 5 held at the end, 0 ruined, 3 faded
  wars: 0 declared, 6 defended; won 0, lost 5, drawn 1, open 0
  adversaries: 6
    The Warrens of Skruz-Snugg (c2): 3 wars, won 0, lost 2, drawn 1, open 0, 2605-3942; now at peace
    The Gnokkirk Horde (c3): 3 wars, won 0, lost 3, drawn 0, open 0, 2594-3934; now at peace
    without war: 4 (enmity 4, none 0)
  events: 89
  fate: alive in 6000: 2 regions, 5 settlements, population 7960
  milestones:
    0 e2 civ_founded: The Elves founded the Realm of Lecier in Stagbarrow Tangle.
    0 e3 settlement_founded: The Realm of Lecier founded Vaenies in Stagbarrow Tangle.
    150-1449 e8 quiet_stretch: For 1,300 years, from 150 to 1449, the land was quiet; one realm held it. (The Realm of Lecier: 8 new generations of rulers.)
    1675 e20 colony_founded: The Realm of Lecier sent settlers from Simi to found Arvies in Yewholt.
...
[present]
year: 6000
civilizations alive: 11 of 11
...
c10 The Warrens of Beenkgnazz: 3 regions, 6 settlements, population 11943
c11 The Bink-Zark Mob: 1 region, 1 settlement, population 2363
regions: 51; held 30, free 21
settlements: 98; thriving 80, declining 7, ruined 8, faded 3
settlement kinds: hamlet 2, village 28, town 58, city 2, fortress 8
largest settlement: Bickton (s73), population 16647
ruins: 8
  s7 Gnix in The Fallen Downs (rg44): ruined 5894, e603 sacked by The Duchy of Rudgor (c7); last held by The Gnokkirk Horde (c3)
  s22 Grix-Kruzz in The Reaped Barrows (rg32): ruined 5882, e591 sacked by The Gnokkirk Horde (c3); last held by The Duchy of Rudgor (c7)
  s26 Gnab Pit in The Forsaken Steppe (rg18): ruined 2610, e95 overrun by Krubgeenk of the Fells (m2); last held by The Warrens of Skruz-Snugg (c2)
  s62 Tisiask in Knight's Weir (rg41): ruined 5147, e350 abandoned after plague, survivors to Favaewhae Glade (s65); last held by The Miaflie Court (c6)
  s68 Sotwee Glade in Knight's Weir (rg41): ruined 5720, e487 abandoned for fear of Morolie the Gold-Eyed (m9), survivors to Fiwiety Hollow (s63); last held by The Miaflie Court (c6)
  s71 Twaess in Hoarscar Weald (rg43): ruined 5714, e479 overrun by Morolie the Gold-Eyed (m9); last held by The Miaflie Court (c6)
  s75 Lallsae in Birchbrake (rg16): ruined 5749, e497 sacked by The Hold of Girinnam (c4); last held by The Liemouvie Ring (c5)
  s84 Shie Hollow in Ugalzakh's Shadow (rg12): ruined 5845, e556 overrun by Ugalzakh the Gaunt (m11); last held by The Hold of Girinnam (c4)
hidden refuges: 0 of 0 founded by flight
open wars: 0
standing claims: 6
  e288 4271 The Realm of Lecier (c1) on The Gnokkirk Horde (c3): The Reaped Barrows (rg32)
  e376 5486 The Realm of Lecier (c1) on The Warrens of Skruz-Snugg (c2): Hound's Wolds (rg25)
  e418 5638 The Warband of Uuld'thu (c8) on The Duchy of Rudgor (c7): The Reaped Barrows (rg32)
  e619 5909 The Warrens of Skruz-Snugg (c2) on The Gnokkirk Horde (c3): The Forsaken Steppe (rg18)
  e629 5928 The Skradreeg Horde (c9) on The Liemouvie Ring (c5): Birchbrake (rg16)
  e675 5989 The Warrens of Beenkgnazz (c10) on The Duchy of Rudgor (c7): The Leas of Smoke (rg31)
renamed regions: 3
  rg12: Ugalzakh's Shadow since 5845, e558 after e556 settlement_overrun; formerly Northeast Wood
  rg18: The Forsaken Steppe since 2610, e96 after e95 settlement_overrun; formerly Brandgap Plains
  rg44: The Fallen Downs since 5894, e605 after e603 sacked; formerly South Downs
[monsters]
monsters: 13; awake 0, asleep 3, slain 5, died 5
  m9 Morolie the Gold-Eyed: Dragon, asleep, lair The Lair of Morolie (si16) in Hoarscar Weald (rg43), since 5672 e447, raids 4, killed 504, hoard 116
  m12 The Restless of Gnix: Undead, asleep, lair The Howes of Gnix (si22) in The Fallen Downs (rg44), since 5895 e606, raids 2, killed 387, hoard 0
  m2 Krubgeenk of the Fells: Troll, died, lair Krubgeenk's Hollow (si7) in The Forsaken Steppe (rg18), since 900 e10, raids 5, killed 482, hoard 84, ended 2894 e132
  m8 The Dead of the Reaped Barrows: Undead, slain by The Duchy of Rudgor (c7), lair The Barrows of the Reaped Barrows (si13) in The Reaped Barrows (rg32), since 5669 e445, raids 5, killed 769, hoard 0, ended 5698 e464
  and 9 more
[sites]
sites: 26; open 8; lairs 16, outposts 10
[peoples]
pairs: 15; met 15; hatred 4, wariness 9, friendship 2
mixed: 3 settlements at E; refugees: 3 arrivals, 764 people; expelled: 0 times, 0 people
```

The summary of this world is 674 lines: the ages, a chronicle of the 161 most notable of its 600 events with every war's declaration, deciding battle and ending, a block per civilization with its wars and adversaries, and the present day with its ruins, refuges, open wars and standing claims.

That hash is the same on every supported compiler and platform, and with any number of threads. A seed phrase is scrubbed before use (lower-cased, with everything other than a to z and 0 to 9 treated as a space), so `"the-unmapped!"` and `"THE  UNMAPPED"` give this same world. The rendered map, about 16 km across with peaks near 2,000 m, three passes, four lakes and about 200 river sources:

![Shaded relief map of the world generated from the phrase "The Unmapped"](docs/images/p1-elevation-map.jpg)

The rainfall layer of the same world (`render --layer rainfall`): dark is wet. The prevailing wind decides which side of each range gets the rain:

![Rainfall map of the same world](docs/images/p1-rainfall.jpg)

The biome map (`render --composite biomes`): dark green is forest, pale green grassland, straw yellow steppe, blue-green wet woodland along the large rivers, pale teal marsh, grey-brown fen, mauve highland moor, rust brown heath and pale grey alpine ground. This world drew a temperature offset of −1.0 °C, so it is a cold one:

![Biome map of the same world](docs/images/p1-biomes.jpg)

Habitability (`render --layer habitability`), here drawn over the relief: brighter land is better to live on. About 43% of this world's land is habitable and 63% can be walked, in two separate areas:

![Habitability map of the same world](docs/images/p1-habitability.jpg)

Regions (`render --composite regions`): the biome map with the borders of this world's 51 regions in black. Borders follow ridge crests in the mountains and large rivers in the lowlands:

![Region map of the same world](docs/images/p1-regions.jpg)

A 1024 world takes about 9 seconds on six cores. `--until elevation` stops after elevation, `--until none` writes an empty world, and `--test-pattern` writes three small test layers that exercise hashing, noise and the layer codec.

| Command | Does |
| --- | --- |
| `generate` | Writes a world file from `--seed N` (decimal or `0x` hex) or `--seed-phrase TEXT` (scrubbed; a phrase is never read as a number). Options: `--out`, `--map-size 512\|1024\|2048`, `--settings FILE`, `--data DIR`, `--until STAGE\|none`, `--threads N`, `--test-pattern`, `--force` |
| `report` | Prints the metadata, row counts per table, each layer's type, size and value range, and geography statistics |
| `render` | Exports a layer as a PNG with `--layer NAME` and a palette, or a shaded relief map with `--composite map`; `--scale 1..16` |
| `hash` | Prints the SHA-256 of the file (equal to `sha256sum`). `--detail` adds separate hashes of the schema, each table and each layer, to locate a difference; `--expect HEX` checks against a known value |

Output goes under `~/.cache/the-unmapped/out/` unless `--out` says otherwise, never into the current directory. `worldgen-cli <command> --help` lists every option.

## Build and run

Requirements: CMake 3.25 or newer, Ninja, and GCC, Clang or MSVC with C++20. Everything else is vendored in [`extern/`](extern/README.md); the build never uses the network.

```
cmake --preset linux-gcc-release
cmake --build --preset linux-gcc-release
ctest --preset linux-gcc-release
```

Presets: `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-clang-release`, `linux-clang-asan`, `windows-msvc-debug`, `windows-msvc-release`.

**Build output lives outside the source tree.** The build tree is `~/.cache/the-unmapped/build/<source dir name>/<preset>/` (`%LOCALAPPDATA%\the-unmapped\...` on Windows), and an in-source build is refused. The command-line tool is at `tools/worldgen-cli/worldgen-cli` inside the build tree.

Other checks:

```
python3 -B tools/lint/determinism_lint.py                              # banned-construct lint
tools/ci/world-hashes.sh local linux-gcc-release linux-clang-release   # compare world hashes across built presets
```

**The world viewer (optional).** The Godot extension is off by default, so nothing above needs Godot. To build it and open a world in the viewer:

```
tools/ci/fetch-godot.sh                                   # downloads and verifies the pinned Godot 4.7.2 editor into ~/.cache/the-unmapped/tools/
cmake --preset linux-gcc-release -DUNMAPPED_BUILD_GDEXT=ON
cmake --build --preset linux-gcc-release
B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-release
"$B/tools/worldgen-cli/worldgen-cli" generate --seed 42 --test-pattern --out ~/.cache/the-unmapped/out/seed42.db --force
~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64 --path "$B/game" -- ~/.cache/the-unmapped/out/seed42.db
```

The viewer lists every layer and composite view in the file in groups, shows the selected one as a map with pan and zoom beside a legend, describes the cell under the mouse (coordinates, value, biome and region name), and lists the metadata. Godot always runs on a copy of the project assembled in the build tree, never on `game/` itself.

[`CLAUDE.md`](CLAUDE.md) has the full build and test reference: every preset, the test executables and their run times, the lint, the hash comparison script and where golden values live.

## How it is built to be reproducible

"The same seed gives the same world" is a hard rule, and it means byte-identical files on Linux and Windows with GCC, Clang and MSVC. Floating-point results, standard-library random distributions and container iteration order all differ between platforms, so the generator avoids them:

- **Integer and fixed-point math only** for anything that reaches the world file. No floating point, no standard-library math functions.
- **One project random generator**, with a separate stream for each stage derived by hashing the seed with the stage's name. Anything keyed by location hashes the seed with the coordinates, so results never depend on evaluation order.
- **Fixed order everywhere.** No iteration over unordered containers, no dependence on pointer values, thread timing or the clock.
- **A reproducible database file.** SQLite is vendored at a pinned version with fixed options; the world is built in memory and written out in one step that makes the bytes depend only on the content.
- **Checked continuously.** Golden hashes are compared on every compiler in CI, and a lint rejects banned constructs.

A change that alters output bytes must bump the generator version and re-record the golden values in the same change.

## Repository layout

```
worldgen/     C++ library with no engine dependencies
  include/    public headers: core, db, data, stages, inspect
  src/        core (fixed point, random, hashing, noise), db, data, stages, inspect
  tests/      doctest tests, golden values, and the independent Python references
tools/
  worldgen-cli/   the command-line tool and its integration tests
  lint/           the determinism lint
  ci/             the cross-compiler world hash script
gdext/        C++ GDExtension for Godot: world reader binding and the viewer's logic
game/         Godot project: the world viewer scene (layout only, no scripts)
extern/       vendored third-party libraries at pinned versions
docs/
  design/     the design document and proposals for changing it
  specs/      one approved spec per generator stage
  reviews/    world critic reviews (from P1 on)
```

Vendored libraries: SQLite 3.54.1, doctest 2.5.3, toml++ 3.4.0, stb_image_write 1.18 and godot-cpp 10.0.0 (used only when the Godot extension is built). Versions, sources, licenses and checksums are in [`extern/README.md`](extern/README.md).

## How the project is developed

The code is written by Claude Code agents with narrow roles, coordinated by a lead session and directed by Paul. Separating the roles that write code from the roles that test and judge it keeps a stage from grading itself.

| Role | Does |
| --- | --- |
| Lead | Breaks each phase into stage tasks, assigns them, approves specs, merges finished work |
| Spec writer | Turns a design section into an exact stage spec |
| Generator engineer | Implements one stage from its spec |
| Test writer | Writes the stage's tests from the spec, without reading the implementation first |
| Determinism auditor | Reviews every change for anything that could differ by compiler or platform; owns the lint |
| Validation specialist | Builds every preset, runs seed sweeps, verifies content independently, records golden values |
| World critic | Judges generated maps and histories against the design (from P1 on) |
| Viewer engineer | Builds the Godot extension and the world viewer (from P2 on) |
| Build engineer | Owns CMake, vendored dependencies and CI |

A stage moves through them in order: spec, then implementation and tests written separately, then the determinism audit, then validation, then merge. The merge gate is that tests pass on every build preset, golden hashes are recorded and match across platforms, and the audit passes. Each phase ends with a human review.

In P0 this process caught, before anything reached `main`: a rolled-back stage leaving a gap in layer ids, a Windows-only path where a failed file rename could report success, a case where a failed write to standard output exited with success, and a TOML parser setting that gave different error lists on different compilers.

The role definitions are in [`.claude/agents/`](.claude/agents), and the working rules are in [`CLAUDE.md`](CLAUDE.md).

## Documents

| Document | What it is |
| --- | --- |
| [`docs/design/design.md`](docs/design/design.md) | The design document: the specification for the whole project. Exported from a working document and read-only in this repository |
| [`docs/design/proposals.md`](docs/design/proposals.md) | Proposed changes, open questions and findings to carry back to the design |
| [`docs/specs/`](docs/specs) | The approved spec for each generator stage, with amendments made during implementation |
| [`CLAUDE.md`](CLAUDE.md) | Hard rules, how work is organized, and the build and test reference |
| [`tools/lint/README.md`](tools/lint/README.md) | The determinism lint's rules |
| [`worldgen/tests/reference/README.md`](worldgen/tests/reference/README.md) | The independent Python references and how to regenerate golden vectors |

## License

[MIT](LICENSE). Vendored libraries keep their own licenses, listed in [`extern/README.md`](extern/README.md).
