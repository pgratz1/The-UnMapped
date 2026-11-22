# P4-00 Viewer v1 overview

Status: **draft, 2026-11-22**, for approval by Paul or the lead. Phase P4 Viewer v1. Design doc sections 3 (architecture), 6 (world database), 10 (tech stack, "World viewer", "Working in C++ only"), 12 (open questions "Layer catalogue in the library" and "History movie") and 13 (phase P4 row; P6 adds the 3D heightmap preview). Depends on P2 (viewer v0, documented in `gdext/README.md`; P2 had no stage specs) and on the approved P3 (P3-00 to P3-11, generator 0.20.0, schema 7), in particular P3-00 6.3 and 6.4, P3-04 8.5 and 10, and P3-07 5.1 and 15. Proposals read: "Viewer: layer units and region names come from a copy of the catalogue", "Viewer: a history movie on the 2D map", "Phase P2 Godot bring-up and viewer v0" and "Paul's approval of phase P3 History core, 2026-11-22". Numbers measured for this spec are from default 1024 worlds, seeds 0, 5 and 42, generator 0.20.0 (`~/.cache/the-unmapped/out/p4-00-spec/`).

This overview splits P4 into stage specs and fixes what they share: what the library provides and what the extension does, the year as the viewer's state, the rules every stage follows, how the headless check grows, the performance budget, how the viewer keeps working while P5 changes history, and the exit check as a walkthrough. It designs no stage in detail. Values marked "interim" are this spec's reading where the design is silent; section 11 lists them for Paul.

## 1. Purpose and exit check

P2's viewer shows the geography of a world: every layer, three composites, legends and a cell description. P3 wrote a history into the same file: civilizations, settlements, territory by region and year, wars and an event log with causes and sentence templates. P4 makes that history visible in the viewer: a year that can be scrubbed, the map coloured by who holds each region in that year, settlements and event places as markers, a browser of civilizations and events with their causes, and playback of the years as a movie.

**Exit check** (design 13): "Paul can follow one civilization from awakening to the present without leaving the viewer." Section 10 turns it into a walkthrough and says which parts the headless check covers.

**Not in P4:** the 3D heightmap preview (P6), the create-world screen and the in-game map (later milestones), figures, factions, monsters and sites as entities (P5 and P6; section 9 says how they appear when they arrive), any change to a world file, and video encoding (section 6.2).

## 2. What the viewer reads

Everything comes from the world file, through `worldgen` (rule V3). Sizes at 1024 with default settings, seeds 0 / 5 / 42:

| Table | Rows | Used for |
| --- | --- | --- |
| `event` (+ `event_participant`, `event_value`, `event_cause`) | 438 / 851 / 745 (939 / 1,816 / 1,589 participants) | the log: sentences, places, participants, causes; at most 7 events in one year (seed 5) |
| `event_kind`, `event_kind_role`, `event_kind_value` | 40 kinds | the catalogue and templates the world carries (P3-00 A1) |
| `civilization` | 8 / 9 / 12 | browser, colours, territory legend |
| `settlement` | 78 / 115 / 102 | markers: cell, founding, end state |
| `settlement_census` | 907 / 1,693 / 1,363 | settlement state by year (P3-00 6.4) |
| `region_control` | 32 / 55 / 51 | holder of each region by year |
| `relation` | 20 / 76 / 77 | wars, truces and enmities at a year |
| `phase`, `age` | 17 / 19 / 24; 6 | the scrubber's marks and headings |
| `region`, layer `region` (1024 × 1024) | 49–56 | names, seed cells (event places), the territory image |
| `pass` | 2 / 2 / 1 | pass markers |

The semantics of a year are P3-04 10's: the state at Y is the state at the end of year Y, after its events; Y runs over [0, E] with E = `meta.history_end_year`; an event belongs to Y when `year` ≤ Y ≤ `end_year`; events of a quiet step are dated at the step's first year. `report --history --level full` takes 0.01 s on seed 5, so reading and digesting the whole history on open is cheap.

## 3. Stage specs of P4

### 3.1 Breakdown

| Spec | Title | Side | Builds it | Depends on |
| --- | --- | --- | --- | --- |
| P4-01 | Timeline in the library | `worldgen`, `worldgen-cli` | spec writer, generator engineer, test writer, determinism auditor, validation specialist | P3 |
| P4-02 | Layer catalogue and region names in the library | `worldgen`, then `gdext` | the same five, then the viewer engineer for the swap | P2 |
| P4-03 | History binding, year and territory | `gdext`, `game/viewer/` | viewer engineer (headless check), validation specialist | P4-01 |
| P4-04 | Settlement and place markers | `gdext`, `game/viewer/` | viewer engineer | P4-03 |
| P4-05 | Browser of civilizations and events | `gdext`, `game/viewer/` | viewer engineer; world critic on screenshots | P4-03 (P4-04 for place highlights) |
| P4-06 | History movie | `gdext`, `game/viewer/` | viewer engineer; world critic on a recording | P4-04 |

**P4-01 Timeline in the library.** Adds to the `worldgen` public interface a read-only timeline built from the history tables that `read_history_tables` already reads and checks (`worldgen/history/timeline.hpp`, interim name): the holder of every region at Y, the state of every settlement at Y (exists, owner, kind, status, population), the relations open at Y, the phase and age of Y, the events of Y, the events of a civilization, region or settlement in id order, and the inverse of `event_cause` (the events an event caused). It also fixes the **civilization colours**, an integer function of the stored world (section 6.3), and a year-dependent composite `territory` (the `regions` composite tinted by holder) with `worldgen-cli render --composite territory --year Y`. Paul sees, after the merge, territory images of any world at any year from the command line. Reads the tables of section 2. Changes no world byte and needs no `GENERATOR_VERSION` bump; adds golden vectors (timeline answers and territory pixel hashes of fixture and shipped worlds, from the test writer's independent reference) and a P0-04 amendment for `--year`.

**P4-02 Layer catalogue and region names in the library.** Answers the first open question of design 12 (section 6.1): `worldgen` gains the public layer catalogue of P1-00 4 (each layer's group, unit, decimal scale, special values and display form, and the legend of each palette and composite), and `RegionRecord` gains `name`; the viewer deletes its copy (`gdext/src/viewer/layer_catalogue.cpp`) and its raw read of the region name column. Paul sees no change in the viewer, which is the point: the headless check passes unchanged (every legend, unit and description identical), and `worldgen-cli render --help` or a new `report` listing may show the units (P4-02 decides; a `report` change is a format change). Independent of P4-01 and can run in parallel with it and with P4-03.

**P4-03 History binding, year and territory.** The extension reads the history once on open through `worldgen` (`HistoryView::load`, `read_history_tables`, `digest_history`, the P4-01 timeline) and keeps it in the model; a world without history opens as today with the time controls disabled. The **year** becomes the viewer's state (section 5, V6): a year bar under the map with a slider over [0, E], a year field, steps of ±1 and ±25 years, previous and next event, and marks for awakenings, phases and the present, with the age and phase of the year shown beside it. A new view `territory` (first under Composites when the world has history) is redrawn when the year changes; its legend lists the civilizations alive at Y with their colours; the cell description gains "held by <civilization> since <year>" at Y. Paul sees the realms grow and shrink as he drags the slider. Adds nothing to `worldgen`.

**P4-04 Settlement and place markers.** An overlay drawn above any view, toggled in the sidebar: one marker per settlement that exists at Y, its shape by kind (hamlet to city, fortress), its fill by the owner's colour, its size by population band, declining shown as an outline, ruined as a ruin symbol, faded ruins hidden unless asked for; hovering a marker describes the settlement at Y. **Site markers before P6** (interim; P6 derives sites): (a) ruins, which P6 turns into sites; (b) passes, from table `pass` at their col cell; (c) event places: events of Y with a place are marked for the year at the settlement's cell, or at the region's seed cell (`region.seed_x`, `seed_y`) when the event has only a region, with a symbol by kind key for battles, sieges, sackings and plague and a plain mark for any other key. Markers are drawn by the extension (Godot drawing, floats allowed, V4); their positions and states come from the timeline. Adds nothing to `worldgen` unless the marker list itself is wanted in `worldgen-cli` (P4-04 may propose it to P4-01 as an amendment).

**P4-05 Browser of civilizations and events.** A browser panel with two lists. Civilizations: name, race, founded and ended years, fate; selecting one shows its block as the history report shows it (peaks, settlements, wars line, adversaries, milestones; all from `digest_history`), highlights its regions on the map at Y, and turns on **follow mode**: the event steps of the year bar move through that civilization's events only. Events: the summary set of `digest_history` by default, all events on request, filtered by civilization, region, settlement or year span; each line is the `render_event` sentence with its `history_note`, like the chronicle line of the report. Selecting an event sets the year to its year, highlights its region and settlement on the map, and shows its participants and values as links (to civilizations, settlements and regions), its causes and the events it caused as links (cause links go backward and forward; a back button returns along the path taken). Selecting a region or settlement (in a link or by clicking the map) filters the list to it. Adds nothing to `worldgen` beyond P4-01.

**P4-06 History movie.** Playback of the year bar (Paul's request, proposals "Viewer: a history movie on the 2D map"): play, pause, speed in years per second, a "skip quiet years" setting that crosses quiet intervals and years without events at a faster rate, and the sentences of each year in a ticker beside the map with the event places flashing (P4-04). Command-line options to start playing (`--play FROM TO`, `--speed`) and to quit at the end, so that one command records a movie with Godot's Movie Maker mode (section 6.2). Adds nothing to `worldgen`.

### 3.2 Order and parallel work

- **P4-01 and P4-02** first, in parallel: the two library stages, each with its own spec, engineer, test writer and audit. They touch different files (`worldgen/history/` and `worldgen/inspect/` against the layer catalogue beside `registered_palette`), and neither touches the P5 subsystems.
- **P4-03** after P4-01 merges. It is the first stage Paul can judge in the viewer. Its spec can be written while P4-01 is implemented, since the overview fixes the interface's content.
- **P4-02's viewer swap** can land in any viewer stage after P4-02 merges, or as its own small branch; it must not be held for a later stage.
- **P4-04 and P4-05** after P4-03, in parallel if two viewer engineers are available (they touch different panels of the screen; the shared parts, the model's year and the selection, are fixed by P4-03). One viewer engineer takes P4-04 first, since P4-05 highlights places with its markers.
- **P4-06** after P4-04; it can run alongside P4-05.

Viewer stages go through the viewer engineer, the headless check on both CI extension jobs (`gdext-linux`, `gdext-windows`) and the validation specialist (every preset, with and without the extension); the determinism auditor reviews them only for what they add to `worldgen` (nothing, by the breakdown). The world critic reviews screenshots and the walkthrough at P4-05 and P4-06 (design 13: a stage that changes how history reads).

No P4 stage changes a world byte, so P4 bumps no `GENERATOR_VERSION` and no schema version. The viewer stages add no golden file; the library stages add golden vectors of their own.

## 4. What exists, and what P4 does not change

`WorldFile`, `WorldLayer`, `ViewerModel`, `LayerMapView` and `ViewerScreen` (`gdext/README.md`, "Classes") stay; P4 extends them and adds panels. The model (`ViewerModel` and the classes it owns) holds the state and every answer; Controls only present it (as P2: no Controls in the model, so a VR view can reuse it). The run copy of the project under the build tree, the pinned Godot 4.7.2 and godot-cpp 10.0.0, and the `UNMAPPED_BUILD_GDEXT` option are unchanged.

## 5. Shared rules

| Rule | Requirement |
| --- | --- |
| V1 C++ only | no GDScript; scenes in `game/viewer/` hold layout only; every behaviour in `gdext/src/` (hard rule 3) |
| V2 Read-only | the viewer opens `world.db` read-only through `worldgen::db::WorldReader` and never writes it, nor anything beside it; viewer settings, if any, go to Godot's user folder, never into the world or the source tree |
| V3 One reading of the world | the extension reads history only through `worldgen`'s public API: `HistoryView`, `read_history_tables`, `digest_history`, `history_note`, `render_event` and P4-01's timeline. It issues no SQL of its own on history tables and restates no rule of the report (salience, the summary set, milestones, notes, the year semantics of section 2). So the viewer and `report --history` cannot disagree, and a P5 change to a table is made once, in the library, by the engineer who changes the table |
| V4 Pixels | the viewer need not draw the same pixels on every platform (scaling, markers, fonts are Godot's). Every image it takes from `worldgen` (layers, composites, `territory`) is worldgen's image unchanged, so the headless check can compare it with `worldgen-cli render` pixel for pixel |
| V5 Library code | any code P4 adds to `worldgen` (timeline queries, colours, the `territory` composite, the layer catalogue) follows hard rule 1 and P3-00 4.5 H1, H3, H5 (integers, defined order, stated tie-breaks), passes the determinism lint, is tested headless by doctest with an independent reference, and never depends on Godot (hard rule 2) |
| V6 The year | the model holds one year Y in [0, E]; every year-dependent view, panel and marker answers that Y; changing Y is one model call with one signal (`year_changed`), and nothing caches an answer across a change of Y. A world opens at Y = E (interim, question 3); without history the year controls are disabled and every view is as in P2 |
| V7 Kinds by key | the viewer never tests an event kind by id and never needs a kind to be known: every sentence comes from the template the world carries. Presentation by kind (a marker symbol, a flash) is a table keyed by kind key with a default for any other key |
| V8 Nothing generated in the tree | worlds, screenshots, recordings and check output go under the build root (`out/`, or the build tree for the headless check), never into the repository |
| V9 One headless check | every new panel is driven by the existing `gdext-headless-check` (section 7) in the same run, through the real scene, not through a test scene of its own (design 10: one check per screen; P4 adds panels to one screen) |
| V10 Errors | a history the library refuses (`history_missing` excepted, which is a world without history) is shown in the file label with worldgen's code and message, the geography views stay usable, and the year controls are disabled |

## 6. The two open questions of design 12

### 6.1 Layer catalogue and region names in the library

**Recommendation:** yes. `worldgen` exposes the layer catalogue (P4-02) and the region name (`RegionRecord::name`), and the history names it already exposes through `HistoryView` stay where they are. The create-world screen, the in-game map and the game all need the same units and names; a copy in each screen drifts (proposals, 2026-11-18). The catalogue is text and integers, so it is cheap to test headless.

**Event text rendering lives in `worldgen`** (already settled by P3-00 A1 and P3-07 15; this overview confirms it for P4): `render_event`, `history_note` and the digest are library code, the viewer calls them, and nobody re-implements template rendering. The viewer adds only layout (links around the names that the participants name).

**Interim**, until P4-02 merges: the viewer keeps its copy and its raw read of the region name, as now.

### 6.2 The history movie and video

**Recommendation:** the movie is P4-06, the last P4 stage, as playback of the year bar; it is cheap once the scrubber, territory and markers exist, and it is the most direct form of "follow a civilization". **Video export is not built in P4**: Godot 4 has a Movie Maker mode (`--write-movie <file>` with a fixed frame rate, writing AVI with MJPEG or a PNG sequence) that records any scene, so P4-06 only adds the options that let one command play a span and quit (`--play`, `--speed`, quit at the end), and documents the recording command in `gdext/README.md`. The output path must be under the build root (V8). Movie Maker needs a rendering display (on a machine without one, `xvfb-run` as for `--screenshot`); P4-06 confirms that it works with the pinned 4.7.2 on Linux and Windows, and no CI job records a movie.

**Interim:** as recommended. If Movie Maker proves unusable, the fallback is a frame dump of the model's images to PNG files, not an encoder in the extension (an encoder would be a new vendored dependency).

### 6.3 Civilization colours (decided here for P4-01, reviewable by Paul)

Colours are a `worldgen` function of the stored world, not stored in it, the same in the CLI and the viewer: each race has a hue family (interim: elves green, goblins yellow-brown, dwarves red-brown, faeries violet, humans blue, orcs red), and the civilizations of a race take distinct shades of it by ascending id. A map then reads first as peoples, then as realms. P4-01 fixes the RGB values and the shade rule (at least four distinguishable shades per race; a fifth and later civilization of one race reuses shades in order). Free land shows the `regions` composite unchanged.

## 7. The headless check, stage by stage

The check runs the real viewer scene on generated worlds without a display (`gdext/README.md`, "Headless check"). It can assert what the model holds and hands to Controls: images, list contents, texts, states, signals; it cannot judge layout or legibility (proposals, P2 item 7). Its real world (`--seed 42 --map-size 512`, kept) has history worth checking: at 0.20.0, 630 events, 10 civilizations of which 2 ended, 30 wars, 21 ruined settlements and 12 regions that changed hands. From P4-03 the check asserts that the world has at least one region changing hands, one war, one ruin and one ended civilization, so that a generator change that removes them fails the check instead of weakening it (the remedy is another seed, chosen by the viewer engineer and recorded in `gdext/README.md`).

| Stage | Added assertions |
| --- | --- |
| P4-02 | none new: every existing legend, unit and description assertion passes with the library catalogue (the assertions themselves move from the viewer's copy to fixed expected strings) |
| P4-03 | the year range is [0, E] from `meta`; the world opens at E; for a set of years (0, each awakening year, the first year a region changes hands, E), the `territory` image equals `worldgen-cli render --composite territory --year Y` pixel for pixel; the territory legend lists exactly the civilizations alive at Y; the cell description names the holder at Y for a held and a free region; the age and phase shown for Y match `report --history` `[ages]` and `[phases]`; a world without history (the test pattern) opens with the year controls disabled |
| P4-04 | at E, the markers by kind and status equal the counts of `report --history` `[present]`; at the first year a settlement is ruined, it has the ruin symbol; markers of event places at Y equal the events of Y with a place; passes equal table `pass`; hovering a marker describes the settlement at Y |
| P4-05 | for every event id, the viewer's sentence and note equal the text of the event's chronicle line in `report --history --level full`; the civilization list equals `[civilizations]` (names, years, fate); in follow mode, stepping from the first event of a chosen civilization to E visits exactly the events it takes part in, in id order (their count equals its `events` in the digest); following a cause link and the back button returns to the start; selecting an event sets Y to its year |
| P4-06 | playback from Y₀ to Y₁ at a fixed speed with a simulated clock (no wall time in the check) visits the expected years, stops at Y₁ and emits the end signal; "skip quiet years" visits only the years with events or holder changes plus the phase starts; the `--play` options parse and are refused clearly when out of range |

The check stays one CTest entry under 60 s on `linux-gcc-release` (300 s timeout). Text comparisons go through `worldgen-cli report --history`, a separate process and a separate code path from the viewer's model, so the check compares two callers of the library, not the library with itself; the library's correctness is the doctest suites' job (V5).

## 8. Performance budget

Release build, Paul's 6-core machine, default 1024 world (about 450–850 events, 80–115 settlements, 30–55 control intervals, 900–1,700 census rows, a 1024 × 1024 region layer):

| Operation | Budget at 1024 | 2048 |
| --- | --- | --- |
| Open a world with history, over the geography-only open | 0.5 s | 2 s |
| Answer a year (holders, settlement states, relations, events) | 2 ms | 8 ms |
| Rebuild the `territory` image for a new set of holders | 40 ms | 160 ms |
| Year change to the next drawn frame while dragging the slider | 50 ms (20 updates a second) | 200 ms |
| Playback | 30 frames a second at any speed up to 200 years a second | 15 |
| Browser: filter or re-list events | 20 ms | 50 ms |

Rules that make this possible: the history is read once on open and held in memory (V3, no SQL per year); a year answer is a binary search per region and per settlement over intervals sorted at load; the territory image is rebuilt only when the holder vector differs from the one drawn (holders change about 50 times in 6,000 years, so playback rebuilds it rarely), from a region → colour table over the region layer; markers are redrawn per frame and are cheap (about 100). Debug builds may take four times as long; no budget applies under a sanitizer. Each viewer stage's validation note records the measured times; a budget missed by more than a factor of two is a defect. P4-01 adds a skipped doctest bench (as `engine-bench`) for the year answer and the territory image; the headless check prints the open and year times as information, never as a pass condition (timing in CI is unreliable).

## 9. Living with P5

P5 History depth runs in parallel and will add tables, columns, event kinds and entity types (figures, factions, monsters; sites with P6), with schema bumps (P5-00, being written).

- **Schema.** Readers accept exactly the current schema version and there are no migrations (design 6). The viewer links the same `worldgen` it is built with, so a viewer built from a commit opens the worlds of that commit's generator. A world from a newer or older schema is refused with worldgen's `unsupported_schema_version` message ("world schema version X, this build reads Y") in the file label, as today; the fix is to rebuild the viewer or regenerate the world. No P4 stage adds tolerance for other schema versions.
- **Columns and tables.** By V3 the viewer reads history only through the library, so a P5 change to a table it uses is made in `read_history_tables`, `HistoryView` or the timeline, with their tests, by the P5 engineer; the viewer is rebuilt, not edited. New tables the viewer does not use are ignored.
- **Event kinds.** New kinds render from the stored templates (V7); they appear in the browser and the ticker at once, with the default marker. A kind that should have its own marker is a later one-line change to the viewer's key table.
- **Entity types 6 to 9.** A participant of a type the viewer cannot link is shown by the name `HistoryView` gives it, or as its type and id when it has none; the browser gains a figures list when P5 provides figures in the library (design 10 lists figures in the browser; read as "when they exist", proposals entry).
- **Merging.** P4's library code lives in new files (`timeline`, the layer catalogue) so that P5's work on `src/history/` rarely conflicts with it. When P5 merges a schema bump, the headless check regenerates its world and runs again on the next viewer branch; a P5 merge that breaks the check is a P5 defect.

## 10. The exit check as a walkthrough

Paul does this on a default world at 1024 (interim: `--seed 42`, a history at 0.20.0 with twelve civilizations of which two ended, three secessions, 32 wars and 12 ruined settlements; question 5), built with any release preset and the extension:

1. Generate the world and open it in the viewer (`gdext/README.md` "Opening the viewer"). It opens at the present, year E, on the `territory` view, with settlement markers on.
2. In the browser's civilization list, choose a civilization that awakened early and has ended or is much reduced. Its block shows its race, founding, peak, wars and fate.
3. Press "first event". The year jumps to its founding (or its race's awakening); the map shows its home region in its colour and its first settlement.
4. Step with "next event" in follow mode to the present. At each step: the year bar shows the year, age and phase; the map shows the holders of that year with the civilization's regions highlighted; the event's place is marked; the sentence and note are shown; the causes and caused events are links.
5. At a war or a loss, follow a cause link back to the claim or declaration, then return with "back".
6. Play the span from founding to E as a movie at a speed of Paul's choice, with quiet years skipped.
7. At year E, the civilization's regions, settlements and fate agree with its block in `report --history` (which he never needs to open; the check has compared them).

The headless check covers the data of every step: images at the years in question (3, 4), the follow order (4), the sentences, notes and causes (4, 5), playback (6) and the end state (7). It does not cover whether the screen is legible, whether the colours tell civilizations apart, or whether the story reads well; that is Paul's walkthrough and the world critic's review of screenshots and a recording at P4-05 and P4-06.

## 11. Open questions for Paul

Each with the recommendation and the interim choice the specs follow until he decides.

1. **Layer catalogue in the library** (design 12). Recommendation and interim: yes, in P4-02, in parallel with P4-01; region names too; the viewer's copy goes (6.1).
2. **History movie and video** (design 12). Recommendation and interim: the movie is the last stage, P4-06, as playback of the year bar; no video encoder; recording through Godot's Movie Maker with the viewer's `--play` options (6.2).
3. **Opening year.** Recommendation and interim: a world opens at the present, year E, since that is the world the game will be played in; "first event" and the slider take Paul back.
4. **Civilization colours.** Recommendation and interim: a hue family per race with distinct shades per civilization (6.3), not one unrelated colour per civilization. Paul may prefer colours that separate rival realms of one race more strongly.
5. **The walkthrough world.** Recommendation and interim: `--seed 42` at 1024 with default settings (the seed of the headless check, at the reference size); Paul may prefer his phrase world or seed 5 or 7, whose reports he read for the P3 review.

## Appendix A. Proposals entries (added to `docs/design/proposals.md` with this spec)

- "Viewer v1: the library answers the year, the viewer draws it" (6.1, 6.3, V3, sites and figures before they exist: 3.1 P4-04, 9).
- "Viewer v1: the history movie is playback, recorded with Godot's Movie Maker" (6.2).
