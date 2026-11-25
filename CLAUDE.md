# The Unmapped

A single-player, first-person, open-world RPG whose world and history are generated from a seed. Dark high fantasy, exploration first, flat screen and PC VR. Built on Godot 4 with all code in C++20.

## Read this first

- **Design doc:** `docs/design/design.md`. It is the specification for the whole project. Read the sections relevant to your task before starting.
- **Current work:** the world generator prototype, section 13 of the design doc (phases P0 to P6).
- **Current phases:** P4 Viewer v1 and P5 History depth, started 2026-11-22, in parallel (overview specs `docs/specs/p4-00-viewer-v1-overview.md` and `p5-00-history-depth-overview.md`). P4-01 the timeline (spec `docs/specs/p4-01-timeline.md` r2; no world change) is merged: `worldgen/history/timeline.hpp` (`src/history/timeline.cpp`: `load_history`, `Timeline::build`, `year_state`, `region_holder`), the civilization colours and territory image of `worldgen/inspect/territory.hpp` (`src/inspect/territory.cpp`), and `worldgen-cli render --composite territory [--year Y]` (default year: the present; file `<stem>.territory-<Y>.png`); the audit's advisories A2 and A3 (`docs/reviews/p4-01-timeline-audit.md`) are closed by P4-03. P4-02 the layer catalogue (spec `docs/specs/p4-02-layer-catalogue.md` r2; no world change) is merged: `worldgen/inspect/layer_catalogue.hpp` (`src/inspect/layer_catalogue.cpp`) holds the layer groups, units, value texts, legends, composite descriptions and `region_name`, and the viewer uses it (its own copy is deleted); lint rule `non-ascii-literal`. Of P4-03 the history view (spec `docs/specs/p4-03-history-view.md` r2; no world change) the library part is merged: the explicit colour table and `race_civilization_colour`, `territory_holders`, borders black only between different holders and `render_territory(base, fill, holders, scale[, out])` in `worldgen/inspect/territory.hpp`, the control-row checks of `Timeline::build`, `detail::fill_buffer` in `src/inspect/render_internal.hpp` and `phase_kind_word`; audit `docs/reviews/p4-03-history-view-library-audit.md` (advisory A1: fill and holders are separate inputs). Its viewer part (spec r3) is merged: `HistoryState` (`gdext/src/viewer/history_state.*`: history status none, loaded or failed), the year bar and `YearMarks` (`year_marks.*`), the territory composite listed first with its legend and hover text, `WorldFile::reader()`, the viewer options `--year Y` and `--bench`, and the headless check's history cases (a geography-only world, territory images at eleven years, checks N1 to N3 and H1 to H8); the world critic's notes on colours, borders and legend (`docs/reviews/p4-03-history-view-world-critic.md`) are closed by P4-03a (change spec `docs/specs/p4-03a-territory-colour-followups.md` r1, P4-03 r4; no world change), which is merged: new values in the colour table (7.2), the inner-border dim 10/16 above luma 175 and 12/16 otherwise, borders between free regions at half the land colour (`TerritoryBase::Cell::half`), the timeline form `render_territory(base, timeline, colours, year, scale[, out])` used by the CLI and the viewer, the legend's border rows with tooltips and its `water` heading, and the viewer's hover text naming a region by `Timeline::region_name` at the shown year; audit `docs/reviews/p4-03a-territory-colour-followups-audit.md` (three advisories). P4-04 settlement and place markers (spec `docs/specs/p4-04-markers.md` r2, P0-04 amendment 24; no world change) is merged: `worldgen/inspect/markers.hpp` (`src/inspect/markers.cpp`: `MarkerIndex::load`, `markers_at`, `capital`, `ruined_year`, `ruin_event`, `marker_glyph`, `draw_markers`), `settlement_kind_word` and `settlement_status_word` in `worldgen/history/digest.hpp`, `read_pass_row` in `worldgen/inspect/geography.hpp`, `worldgen-cli render ... [--year Y] [--markers [--faded]]` (files `<stem>.<name>-<Y>.markers.png`), the file kind `markers.txt` of `world-hashes.sh` (the overlay PNG hashes at the three territory years), golden `worldgen/tests/golden/p4-04-markers.txt` (from `p4_markers_reference.py`), the CTest entries `worldgen-tests-markers` and `-slow-markers` (the slow one takes about 21 minutes at two jobs), and in the viewer the Markers group, key M, markers above every view and marker hover, with the headless checks M0 to M9. P4-05 the browser of civilizations and events (spec `docs/specs/p4-05-browser.md` r1; no world change) is merged. Viewer prelude: `LayerMapView` draws the map as 128 by 128 cell RGBA8 tiles and uploads only changed tiles (Godot has no partial texture update; the slowest year change fell from 170 ms to 21 ms), `--bench` times each step, markers draw at glyph cell 2 for zoom 0.5 to 10. Library (Part A): `render_event_linked`, `TextLink` and `LinkedText` in `worldgen/history/event_catalogue.hpp` (`render_event` is its text); `civilization_block`, `Segment`, `BlockLine`, `block_line_report_text`, `block_line_display`, `history_entity_tag` and `history_report_digest` (the one path to the report's summary set) in `worldgen/inspect/history_report.hpp`, with the report's `[civilizations]` blocks printed from the block lines; `worldgen/inspect/history_browser.hpp` (`src/inspect/history_browser.cpp`: `BrowserIndex` with `build`, `select`, `event_detail`, `row_text`, `block`, `search`; `EventFilter`; six kind groups, `monsters` = 5 for kinds 44 to 55, `ALL_GROUPS` 0x3F, a lead ruling the spec's 4.4 still has to record); `MarkerIndex::event_place`; the ruin and faded-ruin glyphs and the plague ring colour (0,190,120) of the world critic's F3 and F5; `monster_tables()` refuses non-consecutive event-kind ids. Golden `worldgen/tests/golden/p4-05-browser.txt` (from `p4_browser_reference.py`, fixture `worldgen/tests/data/browser/t3-browser.toml`), CTest entries `worldgen-tests-browser` and `-slow-browser`; `p4-04-markers.txt` re-recorded (rules `p4-05`). Viewer (Part B): `BrowserState` (`gdext/src/viewer/browser_state.*`), `viewer_model_browser.cpp`, `viewer_screen_browser.cpp`, the right pane with Browser and Legend tabs, the dimming mask, frames and event-place squares of `LayerMapView`, keys B, F, Backspace, Shift+Backspace, Escape and Ctrl+F, the options `--select`, `--browser-group`, `--browser-full`, `--search`, `--tab` and `--size`, and the headless checks B0 to B12 (`viewer_check_browser.*`). Audit `docs/reviews/p4-05-browser-library-audit.md` (pass; advisories A1 empty names print as the tag alone, A2 `search` relies on `region_name` row order). Its spec is at r3 (as built, with the kind group `peoples` of P5-03). P4-06 the history movie (spec `docs/specs/p4-06-history-movie.md` r2, as built; viewer only, no world change) is merged: `PlaybackState` (`gdext/src/viewer/playback_state.*`, no Godot types: states, span, speeds 1 to 500 years per second, skip quiet years, stop years, at most one year change per frame and no stop year passed, the ticker of at most 6 entries, the flashes, `parse_play_options`), `viewer_model_playback.cpp`, `viewer_screen_playback.cpp`, the two-row year bar (Play, slider, speed and Skip quiet above the step buttons), keys Space, Shift+Space, `-`/`=`, Q and T, the options `--play <from> <to>`, `--speed`, `--skip-quiet on|off`, `--quit-at-end`, `--play-log` and `--no-pane`, the headless checks P0 to P10 (`viewer_check_playback.cpp`) and the runs R1 to R3 of `headless_check.cmake`; a recording with Godot's Movie Maker mode is deterministic frame for frame. Movie Maker writes frames at the project's 1600 by 900 whatever `--resolution` says, so a recording command must use 1600x900. Open: a bench line for the stop-year recomputation (the ticker stays over the map's lower third). With P4-06 every stage of P4 is merged; Paul reviewed the running viewer and approved P4 on 2026-11-24 with no concerns, so the interim choices of the open questions of P4-04 (section 16), P4-05 (section 14) and P4-06 (section 14) stand as built. P5-01 carried-over follow-ups (spec `docs/specs/p5-01-followups.md` r1; generator 0.21.0, history report format v2) is merged; its audit advisory A1 and the world critic's notes F1 to F3 (`docs/reviews/p5-01-followups-world-critic.md`) are closed by P5-12. P5-12 regions renamed by history (spec `docs/specs/p5-12-renamed-regions.md` r2; generator 0.22.0, schema 8, history report format v3) is merged: `region.name` stays geographic and table `region_name (region, from_year, name, culture, event)` holds the later names; the renaming subsystem `worldgen/history/renaming.hpp` (`src/history/renaming.cpp`) runs after conflict with the tuning id `history.renaming` (`game/data/history/renaming.toml`, six rules, stage check `renaming_rules`) and the streams `hist.renaming.chance` and `hist.renaming.name`; event kind 43 `region_renamed` and the placeholder `{@region_former}`; `Timeline::region_name(region, year)` and `HistoryView::region_name_at_event`/`region_name_at_year`; the wider doubled-suffix rule of the name builder, the name filter at the joins of region names (V8), the report's `renamed regions:` list and `hidden refuges:` line, and `inspect::settlement_row`. Golden file `worldgen/tests/golden/p5-12-renaming.txt` (from `p5_12_renaming_reference.py`). Open after it: the audit's advisories A1 (no check that `region_name` rows ascend) and A2 (`docs/reviews/p5-12-renamed-regions-audit.md`), the world critic's notes (`docs/reviews/p5-12-renamed-regions-world-critic.md`) and P5-01's target T16 (repeated battles: 13 of 8,935, bound 10; a restatement as a rate is proposed in `proposals.md`, Paul's decision). P5-02 beasts and monsters (spec `docs/specs/p5-02-beasts-monsters.md` r2; generator 0.23.0, schema 9, history report format v4) is merged: the subsystem `worldgen/history/monsters.hpp` (`src/history/monsters.cpp`, tuning id `history.monsters`, data kinds `beast` in `game/data/beasts/` and `monster` in `game/data/monsters/`) runs after conflict and before renaming; tables `beast`, `monster_kind`, `beast_population`, `monster` and `site` and the column `settlement.ending` (sacked, overrun, plague, famine, left); event kinds 44 to 55; lairs and outposts as sites; the raid target is a keyed weighted draw (stream `hist.monsters.raid_target`) that prefers small settlements; `holds_settlement` is set only by an overrun (Paul, 2026-11-23: an undead host risen in a ruin may still overrun once); the peoples' ghost rule (`ghost_permille`, `ghost_max_population`); the renaming rule `ravage` with the `{stem}` slot; the report's `[monsters]` and `[sites]` blocks and the summary budget of spec 12.9 (`SUMMARY_LINES_CAP`); the timeline reads `sites_existing`, `site_at`, `monster_at` and `beast_count`; `StepContext::phase_trigger_event`; lint rule `history-member-held-reference`; the `world-hashes.sh` case `p5-monsters-high-1024` (sixteen cases). Golden file `worldgen/tests/golden/p5-02-monsters.txt` (from `p5_02_monsters_reference.py`); CTest entries `worldgen-tests-monsters` and `-slow-monsters`. It closes P5-12's audit advisories A1 and A2 and restates T16 as a rate (at most 2 repeated battles per thousand; 1.60 measured). Reviews: `docs/reviews/p5-02-beasts-monsters-audit.md`, `-validation.md`, `-world-critic.md`. Open after it: `p5_12_renaming_reference.py check-world` gives a false positive when two rules share a cause kind (its `max_weight`; seed 5 at 1024 with `--monster-pressure high`), and the world critic's notes. P5-03 race relations, mixed settlements and migrations, with the P5-02 follow-ups (spec `docs/specs/p5-03-race-relations.md` r3; generator 0.24.0, schema 10, history report format v5) is merged: the subsystem `worldgen/history/relations.hpp` (`src/history/relations.cpp`, tuning id `history.relations`); tables `race_relation` and `settlement_race`, the columns `beast.bold_phrases` and `monster_kind.raid_phrases`, the ending 6 `fled`; event kinds 56 to 62 (`settlement_fled`, `peoples_met`, `disposition_turned`, `refugees_arrived`, `settlement_mixed`, `standing_changed`, `people_expelled`); the streams `hist.beasts.bold_kind`, `hist.monsters.fear`, `hist.peoples.abandon_loss` and `hist.relations.expel`; every movement of people goes through `PeopleLedger::moved` and `displaced`; conflict reads race disposition through `DispositionView`; `Settlement::owner_event`, `StepContext::event_participants`, `RenamingSubsystem::name_now`; a ruin left by plague, famine, leaving or flight never fades; a sentence that starts with a lower-case letter is capitalised; the report's `[peoples]` block; the timeline reads `race_relation_at` and `settlement_races`; the browser's seventh kind group `peoples` = 6 for kinds 57 to 62 (`ALL_GROUPS` 0x7F, kind 56 in `monsters`); both history readers report a race missing from `event_participant` with the same first error (`race_participant_problem`); lint rule `history-member-held-reference` extended; an empty world is 45 pages. Golden file `worldgen/tests/golden/p5-03-relations.txt` (from `p5_03_relations_reference.py`); CTest entries `worldgen-tests-relations` and `-slow-relations`. Targets as restated by the lead and measured on 100 default worlds: all pairs met in at least 80 worlds (84), raids citing an earlier raid at least 30% (38.7%), expulsions median at most 5 (3; max 22), raids on one place median at most 8 and max at most 40 (5; 33), wars within 10% of 0.23.0 (25.5, +8.5%), T16 0.93 per thousand, `civs4` summary median at most 1,150 lines (1,112). Reviews: `docs/reviews/p5-03-race-relations-audit.md`, `-validation.md`, `-world-critic.md`. Open after it, for the next change that alters world bytes or its test writer: the audit's advisories A1 (`mix` can round the ruling share to 0) and A2 (`disposition()` is fatal before the first relations step); three stale reference checks (`p5_03_relations_reference.py check-world` invariant 4 on a colony founded in the year its region is ceded, and lines 1317 and 1726 with the old reader error text; `p5_12_renaming_reference.py` `max_weight`; `p5_02_monsters_reference.py` invariants 5 and 11, superseded by P5-03 9.1 and 9.2); the default summary reaches 906 lines on seeds 100 to 199 (bound 800 on seeds 0 to 99: 785); lairs on former settlements open at the end, 62 and 66 worlds in two samples of 100 against Paul's bound of 65; turns to friendship are rare (3 in 100 worlds against 116 to hatred); the world critic's notes. P5-03a relations follow-ups (change spec `docs/specs/p5-03a-relations-followups.md` r2, P5-03 r3; generator 0.25.0, schema 10 unchanged, history report format v6) is merged: goodwill from a long peace between neighbours (`peace_years`, `peace_memory_per_century`, `peace_memory_max`, `contact_hops`), from a common enemy (`foe_*`) and from shelter given (`shelter_memory`); peoples meeting by word (`meet_sealed_hops`, `meet_sealed_years`); the settlers' share as a keyed draw (stream `hist.relations.settlers`, 77 streams); `expel_gap_years`; the undead threshold on the dead (`undead_dead_permille`, `undead_min_dead`); `history.conflict.border_friction` 300; the public seam `RelationsSubsystem::credit` (a caller passes an event of the current step, audit A4), `DispositionView::band` and `met`, `StepContext::event_causes`, `HistoryView::monster_kind_row`; the renderer forms `its`, `aside` and `where`, a template limit of 1,024 bytes, new values on kinds 57, 60 and 62; the summary's milestones as a count, its ruins list of at most 15 and `SUMMARY_MIXED_MAX` 3; the `[peoples]` block's unmet lines and the stats `peoples_met_by_word` and `dispositions_warmed`; an empty monster root gives no `{name}` in a region name. It closes P5-03's audit advisories A1 and A2 and the summary length (default median 550 lines, `civs4` 867.5). Targets as ruled by the lead, measured on default seeds 0 to 99 (and 100 to 199): turns into friendship at least 25% (33%, 36%), at most 20 turns in a world (16, 13), turns before year 5500 at least 55% (57%, 54%), wars median 23 to 28 (25, 25), T16 at most 2 per thousand (1.65, 0.98), standing settlements at least 81 and population at least 205,000 (85 and 211,977), expulsions median at most 3 (2; max 13). Reviews: `docs/reviews/p5-03a-relations-followups-audit.md`, `-validation.md`, `-world-critic.md`. Open after it: P5-02's target M4 (a living monster at the end: 82 and 83 worlds against 85, from the earlier undead; Paul's decision, Q-P5-03a-1 of the questions file); undead almost never rise at 2048 or under `--monster-pressure low` (validation finding 3, not debugged); `p5_03_relations_reference.py check-world` invariant 4 also needs a same-year `settlement_mixed` (seed 66); the audit's advisories A2 to A4; the year-0 `word_since` clock; the slow suite's same-step cession witness is now 1024 turbulent seed 4; the world critic's notes. Before them: P3 History core, started 2026-11-19. The overview spec (P3-00, `docs/specs/p3-00-history-overview.md`) splits it into specs P3-01 to P3-07. P3-01 (history data and settings; generator 0.12.0), P3-02 (names: `src/history/draw.cpp`, `src/history/names.cpp`), P3-03 (chronology; generator 0.13.0, schema 6) P3-04 (simulation engine; generator 0.14.0, schema 7) P3-05 (peoples; generator 0.15.0) and P3-06 (expansion and conflict; generator 0.16.0) are merged. P3-08 names and peoples follow-ups (change spec `docs/specs/p3-08-followups.md`; generator 0.17.0) is merged: region names without numerals (spec P1-06, data kind `placename` in `game/data/placenames/`), a lower-case "the" mid-sentence, the name filter (spec P3-02 section 11, data kind `name_filter` in `game/data/name_filters/`, source `src/data/kinds_names.cpp`), the faerie forest weights, dwarven upland sites and event kind 31. P3-09 conflict balance (generator 0.18.0; P3-06 section 20, P3-05 section 21, rules decided by Paul in `proposals.md`) is merged: event kinds 32 to 36 (`refugees_settled`, `tribute`, `war_won`, `search_abandoned`, `refuge_held`), the flight of a last refuge, the loser's cost, weariness, concealment (stream `hist.conflict.search`), the history notes of `worldgen/history/digest.hpp` (`src/history/digest.cpp`) and the short English-word list of the name filter. P3-10 wording, site and conflict follow-ups (change spec `docs/specs/p3-10-followups.md`; generator 0.19.0; P3-06 sections 21 and 22) is merged: event kinds 37 and 38 (`plague_outbreak`, `quiet_stretch`), cause clauses, one compass name per land type, faerie and dwarf sites, the conflict rules of a quarrel on awakening (held back below `history.conflict.quarrel_min_settlements`), an aim won only by holding the goal, foothold cessions and the end decision in the order of 21.6 (`end_decision` in `src/history/conflict_war.cpp`), the race field `hidden_years`, one lying-low definition (`PeoplesSubsystem::lie_low`), `history.engine.quiet_min_years`, and the `*-slow` suites in CI. P3-11 orc arrival, wording and name follow-ups (change spec `docs/specs/p3-11-followups.md`; generator 0.20.0; P3-05 section 23, P3-06 section 23, P3-02 section 14, P3-04 section 21) is merged: the race field `awakening_population` and the first colony of an awakened realm free of the pressure test (`first_colony_pending_` in `src/history/peoples.cpp`), event kinds 39 and 40 (`claim_settlements`, `settlements_ceded`), the faerie syllable lists, the repeated-syllable rule of the name builder, `tard` in the name filter and values one to nine rendered as words. Its section 12 lists what is left for the next change that alters world bytes. P3-07 the history report (spec `docs/specs/p3-07-history-report.md` r18; no world change, generator stays 0.20.0) is merged: `worldgen-cli report --db <world.db> --history [--level summary|full] [--out PATH] [--force]`, with the digest in `worldgen/history/digest.hpp` (`src/history/digest.cpp`, `digest_tables.cpp`: `read_history_tables`, `digest_history`, `history_note`) and the text in `src/inspect/history_report.cpp` (`build_history_report`); a world without a history gives `ErrorCode::history_missing` (exit 3). Its text is part of the cross-platform contract and is compared by `world-hashes.sh` at both levels. The world critic's remaining notes (`docs/reviews/p3-07-history-report-world-critic-r17.md` B1 to B6) and the audit's advisory A3 wait for the round after Paul's review. P3 was reviewed and approved by Paul on 2026-11-22. P0 Foundation, P1 Geography (approved by Paul on 2026-11-19) and P2 (Godot bring-up and viewer v0) are done. The reference map size for history is 1024.
- **Next phase:** P6 Derivation and polish, after P4 and P5.

## The design doc is read-only

`docs/design/design.md` is exported from a working document maintained outside this repository, and it is overwritten on every update. Do not edit it.

When the design is wrong, unclear or missing something, add an entry to `docs/design/proposals.md` and carry on with the most reasonable reading. Paul brings proposals back to the design conversation.

## Repository state

The repository follows the layout in section 10 of the design doc. Phase P0 is in place: the `worldgen` library holds the deterministic core, the world database layer, the TOML loader, the P0 test-pattern stage and the inspection code behind `report` and `hash`; `worldgen-cli` has `generate`, `render`, `report` and `hash`. Of phase P1 the shared geography algorithms (`src/geo/`), the stage pipeline and the six geography stages, `elevation` (spec P1-01, `src/stages/elevation.cpp`, with the pass layout in `src/stages/elevation_passes.cpp`; it also writes table `pass`, schema version 2), `hydrology` (spec P1-02, `src/stages/hydrology.cpp`), `climate` (spec P1-03, `src/stages/climate.cpp`: layers `temperature` and `rainfall`, and the one-row table `climate` with the wind and the per-world temperature offset) and `biomes` (spec P1-04, `src/stages/biomes.cpp`: layer `biome` and table `biome`, nine natural biomes; schema version 4) and `resources` (spec P1-05, `src/stages/resources.cpp`: the score layers `fertility`, `ore`, `timber`, `fresh_water`, `forage` and `habitability`, and layer `walk_area`) and `regions` (spec P1-06, `src/stages/regions.cpp`: layer `region` and tables `region`, `region_border` and `region_biome`; schema version 5), are in place, with the required tuning kind `worldgen` (ids `elevation`, `hydrology`, `climate`, `biomes`, `resources` and `regions`) read from `game/data/worldgen/` and the required data kind `biome` from `game/data/biomes/`. Of phase P3 the data kinds `race` (`game/data/races/`), `culture` (`game/data/cultures/`) and the fixed-id tuning kind `history` (`game/data/history/`, id `chronology`) are registered and required (spec P3-01, `src/data/kinds_history.cpp`), and `generate` parses the world-creation settings (`--history-span`, `--history-detail`, `--civs-per-race`, `--races`, `--event-intensity`, `--monster-pressure`, `--divine-activity`; `--divine-script` is refused until P5), which are recorded in `meta` as `param.*` keys. The first history stage, `chronology` (spec P3-03, `src/stages/chronology.cpp`: tables `race`, `race_biome`, `culture`, `god` and `age`; schema version 6), writes the races present, their awakening years, the ages and a creator god per race; `report` prints them in `[chronology]`. `generate` runs the implemented stages by default; `--until none` writes the P0 empty world. `render` also draws the composites `map`, `biomes` and `regions`. The stage `history` (spec P3-04, `src/stages/history.cpp`, with the engine in `src/history/engine.cpp`, the schedule in `schedule.cpp`, the event catalogue in `event_catalogue.cpp` and a placeholder founding rule in `stubs.cpp` kept for tests; schema version 7) writes tables `event_kind`, `event_kind_role`, `event_kind_value`, `phase`, `event`, `event_participant`, `event_cause`, `event_value`, `civilization`, `settlement`, `settlement_census`, `region_control` and `relation`, and `report` prints `[history]`; its tuning is the `history` ids `engine`, `report`, `peoples` and `conflict`. The shipped history is the peoples subsystem (spec P3-05, `src/history/peoples.cpp`, `peoples_geo.cpp`): event kinds 6 to 19, 31 and 37, the engine's kind 38 (`quiet_stretch`), and the conflict subsystem (spec P3-06, `src/history/conflict.cpp`, `conflict_war.cpp`): event kinds 20 to 30, 32 to 36, 39 and 40, claims, wars, battles, sieges, sackings, captures, truces and cessions, with the `war`, `peace` and `enmity` rows of table `relation`. History subsystems follow the rules of spec P3-04 section 6.9 (no founding inside a loop over the live lists, no reference held across a founding; lint rules `history-live-iteration` and `history-held-reference`). History code may use keyed draws only, never the sequential generator (lint rule `history-sequential-rng`).

```
CMakeLists.txt        top-level build; refuses in-tree builds
CMakePresets.json     configure, build and test presets
cmake/                build modules; UnmappedCompilerFlags.cmake holds every compiler flag
worldgen/             C++ library, no engine dependencies
  include/worldgen/   public API
  src/                core/, db/, data/, stages/, inspect/; geo/, history/, derive/ come with their first file
  tests/              doctest tests
tools/worldgen-cli/   command-line front end: generate, render, report, hash
tools/lint/           banned-construct lint (owned by the determinism auditor)
tools/ci/             world-hashes.sh, the cross-compiler world hash check
gdext/                C++ GDExtension: world.db binding and viewer logic (phase P2, viewer engineer)
game/                 Godot project, layout only; the viewer scene is game/viewer/viewer.tscn (phase P2); data/worldgen/ holds the shipped generator tuning
extern/               vendored third-party libraries at pinned versions, see extern/README.md
docs/                 design/, specs/, reviews/
.github/workflows/    continuous integration
```

## Hard rules

1. **Bit-identical worlds.** The same seed, parameters and generator version must give a byte-identical `world.db` on Linux and Windows with GCC, Clang and MSVC. In `worldgen/`:
   - Integer and fixed-point math only for anything that reaches the database.
   - No standard-library transcendental functions or random distributions in the output path. Use the project's own.
   - One project random generator; streams derived by hashing the seed with a stage name or coordinates.
   - No iteration over unordered containers, no dependence on pointer values, thread timing or wall-clock time.
   - No fast-math flags.
   - SQLite at the vendored version, fixed page size and settings, rows written in a defined order, no timestamps.
2. **`worldgen` never depends on Godot.** The library and `worldgen-cli` build and run without the engine.
3. **C++ only.** No GDScript. Godot scenes and resources hold layout, assets and tuning data.
4. **Data in TOML.** Races, beasts, monsters, cultures, biomes and event rules are hand-edited TOML. SQLite is for generated output.
5. **Dependencies are vendored** in `extern/` at pinned versions. Do not add network fetches to the build.
6. **Spec before code.** Each generator stage has an approved spec in `docs/specs/` before it is implemented.
7. **The database schema is a contract.** A schema change needs a schema version bump, a spec update and a proposals entry.

## How work is organized

**Models.** Run the main session on Fable. The subagents in `.claude/agents/` name their model in their frontmatter: Opus for `spec-writer`, `generator-engineer`, `test-writer`, `determinism-auditor` and `viewer-engineer`; Sonnet for `validation-specialist`, `build-engineer` and `world-critic` (Paul, 2026-11-22, to save usage). A validation failure that needs debugging to a root cause may be sent to an Opus agent. Start a fresh agent per task with a tight brief rather than resuming one whose context has grown large.

The main session acts as **lead**: it reads the design doc, breaks the current phase into stage tasks, delegates to the subagents in `.claude/agents/`, and merges finished work. The lead does not write stage code itself.

A stage moves through the roles in this order:

1. `spec-writer` writes the stage spec. Paul or the lead approves it.
2. `generator-engineer` implements the stage and `test-writer` writes its tests, each working from the spec separately.
3. `determinism-auditor` reviews the change.
4. `validation-specialist` builds every preset, runs the tests and a seed sweep, and records golden hashes.
5. `world-critic` reviews the output when a stage changes how a world looks or how its history reads.
6. The lead merges when every check passes. A failed check goes back to the engineer with the defect report.

`viewer-engineer` owns the Godot side, and `build-engineer` owns CMake, `extern/` and continuous integration.

## Merge gate

- Tests pass on every build preset.
- Golden-seed hashes are recorded and match across platforms.
- The determinism audit passes.
- Map images or a history report are attached for any stage that changes output.

One stage per branch, merged when green. Each phase ends with a review by Paul before the next phase starts.

`docs/questions-for-paul.md` is the one tally of every question put to Paul (open, decided, settled by phase approval), wherever it was first asked. The lead adds a stage's new questions and moves answered rows to its decided section at each merge, and records each decision in `proposals.md`; a spec or review that puts a question to Paul must also get a row there.

`README.md` carries the public status and the phase timeline. The lead updates its Status and Timeline sections whenever a stage merges to `main`, a phase starts or ends, or work in flight changes, in the same push.

## Build and test

Requirements: CMake 3.25 or newer, Ninja, and GCC, Clang or MSVC (Visual Studio 2022 or newer) with C++20. Everything else is vendored in `extern/`; the build never uses the network. The optional Godot extension also needs Python 3.8 or newer, and the pinned Godot editor to run it (see "Godot extension and viewer" below).

### Presets

| Preset | Compiler | Build type |
| --- | --- | --- |
| `linux-gcc-debug` | GCC | Debug |
| `linux-gcc-release` | GCC | Release |
| `linux-clang-debug` | Clang | Debug |
| `linux-clang-release` | Clang | Release |
| `linux-clang-asan` | Clang | Debug (`-O1 -g`, assertions on) with AddressSanitizer and UndefinedBehaviorSanitizer; `-O1` keeps the CI job well inside its timeout and changes no world byte |
| `windows-msvc-debug` | MSVC | Debug |
| `windows-msvc-release` | MSVC | Release |

Each preset has a configure, a build and a test preset of the same name; the test presets `linux-gcc-release-slow` and `windows-msvc-release-slow` run only the slow entries (label `slow`, see below), which the other test presets exclude. Linux presets are hidden on Windows and the reverse.

### Commands

Linux, from the repository root:

```
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
```

Replace `linux-gcc-debug` with any preset above. The build tree is `$HOME/.cache/the-unmapped/build/<source dir name>/<preset>/`; the executables below are inside it.

Windows, from an "x64 Native Tools Command Prompt for VS" (or after running `vcvars64.bat`), so that `cl` and Ninja are on the path:

```
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

The build tree is `%LOCALAPPDATA%\the-unmapped\build\<source dir name>\<preset>\`.

### Executables and tests

| Path in the build tree | What it is |
| --- | --- |
| `tools/worldgen-cli/worldgen-cli` | The command-line tool: `generate`, `render`, `report`, `hash` (spec P0-04) |
| `worldgen/tests/worldgen-tests` | doctest tests of the library: core, database layer, TOML loader, test-pattern stage, render, report and hashes |
| `tools/worldgen-cli/tests/worldgen-cli-tests` | doctest integration tests that run `worldgen-cli` as a child process |
| `tools/worldgen-cli/worldgen-cli-test-hooks` | `worldgen-cli` with a test-only fault hook, used by `worldgen-cli-tests`; never shipped |

`ctest` runs each doctest executable as several entries, each a disjoint group of its source files (doctest `-sf=`), so that they run in parallel; the test presets run four at a time (`"jobs": 4`; `ctest --preset <preset> -j 6` overrides it). The groups are declared with `unmapped_add_doctest_shards()` (`cmake/UnmappedTestShards.cmake`) at the end of `worldgen/tests/CMakeLists.txt` and `tools/worldgen-cli/tests/CMakeLists.txt`:

| CTest entry | What it runs |
| --- | --- |
| `worldgen-tests-elevation-golden`, `-elevation`, `-hydrology`, `-climate`, `-biomes`, `-biomes-inspect`, `-resources`, `-regions`, `-history`, `-chronology`, `-chronology-property`, `-engine`, `-engine-r12`, `-engine-worlds`, `-engine-worlds-resume`, `-peoples`, `-conflict`, `-conflict-worlds`, `-balance`, `-history-report`, `-layer-catalogue`, `-timeline`, `-renaming`, `-monsters`, `-relations`, `-markers`, `-browser` | `worldgen-tests` on that stage's test files (`-history`: the keyed draws and names tests under `worldgen/tests/history/`; `-chronology`: the golden, unit and guard files, `-chronology-property`: the property, schema and report files; `-engine-worlds`: the 512 world case and threads, `-engine-worlds-resume`: R1, resume and, in the slow suite, the 1024 cases; `-engine` also holds the skipped suite `engine-bench`; `-engine-r12`: the engine tests of spec revisions 12, 15 and 17; `-balance`: the P3-09 conflict balance tests; `-history-report`: the files `*history_report_*`, the P3-07 history report's fixture, block, corruption and 512 world tests; `-layer-catalogue`: the P4-02 generated-world test; `-renaming`: the files `*history_renaming_*`, the P5-12 renaming tests; `-timeline`: the files `*timeline_*`, the P4-01 timeline and territory tests and the P4-03 colour, holder and border tests) |
| `worldgen-tests-other` | `worldgen-tests` on every file no group names (core, data, database, geo, pipeline, inspection; a new test file lands here until it is given a group) |
| `worldgen-cli-tests-hydrology-elevation`, `-passes-climate`, `-biomes`, `-resources-generate`, `-regions`, `-history-settings`, `-chronology`, `-engine`, `-conflict`, `-territory` | `worldgen-cli-tests` on those files |
| `worldgen-cli-tests-other` | `worldgen-cli-tests` on every other file (among them `cli_history_report_test.cpp`, the P3-07 `report --history` tests) |
| `worldgen-tests-slow-elevation`, `-slow-engine-resume`, `-slow-peoples`, `-slow-conflict`, `-slow-climate`, `-slow-hydrology`, `-slow-engine`, `-slow-biomes-resources-regions`, `-slow-history-report` (the history report's 1024 cases), `-slow-timeline`, `-slow-renaming`, `-slow-monsters`, `-slow-relations`, `-slow-markers`, `-slow-browser`, `-slow-other`; `worldgen-cli-tests-slow-history-regions`, `-slow-geography`, `-slow-other` | label `slow`, not run by the default test presets: the skipped `*-slow` suites (`-ts=*-slow --no-skip`) of those files (`SLOW` lines of `unmapped_add_doctest_shards()`), `-slow-other` every slow case of a file no `SLOW` line names. `ctest --preset linux-gcc-release-slow` (or `windows-msvc-release-slow`) runs them, four at a time; for another build tree, `ctest --test-dir <build tree> -L slow -j 4` (not `--preset <preset> -L slow`: the default presets exclude the label). Not registered under `linux-clang-asan` |
| `worldgen-tests-partition`, `worldgen-cli-tests-partition` | checks with `--count` (and `--count --no-skip`) that the groups and `-other` hold every test case exactly once and that no group is empty, and that the slow entries hold every case of the `*-slow` suites exactly once with no `SLOW` entry empty (the skipped suites `engine-bench`, `peoples-bench` and `p3-08-dump` are run by hand only) |
| `worldgen-cli-version`, `worldgen-cli-unknown-command` | two quick `worldgen-cli` checks |
| `gdext-headless-check` | only with the extension (see below) |

`ctest -R '^worldgen-tests'` runs the library entries only. The executables are unchanged: run one directly for the whole set or for options such as `-tc="<name>"`, `-ts=`, `-sf=` or `-s`. A group keeps a stage's files together, since its support headers cache inputs in memory per process; a group that grows past the others is split by naming fewer files. Every entry is safe to run alongside the others: scratch directories are per process (process id, or created atomically) and caches are in memory. Tests write scratch files under the build tree or the system temp directory, never into the source tree. Each `worldgen-tests-*` and `worldgen-cli-tests-*` entry has a CTest timeout of 3600 s on every preset, and `gdext-headless-check` one of 300 s.

Approximate full `ctest` time on a 6-core Linux machine, with the presets' four jobs:

| Preset | `ctest` |
| --- | --- |
| `linux-gcc-release`, `linux-clang-release` | about 55 seconds |
| `linux-gcc-debug`, `linux-clang-debug` | about four minutes |
| `linux-clang-asan` | about 8 minutes in CI (4 vCPUs; `-O1`, see below) |

Measured 2026-11-19 at generator 0.11.0 on an otherwise quiet machine (`linux-gcc-release` 53 s, `linux-gcc-debug` 223 s, `linux-clang-debug` 235 s, `linux-clang-asan` 996 s). Windows times are not measured locally; CI job times vary by up to a factor of two between runs with the hosted runner's speed (`docs/reviews/ci-job-time-variance.md`). With `-j 1` it takes two to three times as long (measured at generator 0.9.0: release about two and a half minutes, `linux-clang-asan` about 44). Under `linux-clang-asan` the longest entries (`worldgen-tests-elevation-golden`, `worldgen-cli-tests-hydrology-elevation`, `worldgen-tests-hydrology`) take 6 to 7 minutes each, which is the floor however many jobs run. For quick iteration use `linux-gcc-release` or `linux-clang-release` (run a single doctest executable with `-tc=` or `-ts=` for narrower checks); run both debug presets and `linux-clang-asan` before handing work on. The `linux-clang-asan` figure predates the history stages and the `-O1` change: the preset was a plain Debug build and its CI job took about 160 minutes (build 7 to 8, `ctest` 126 to 131, `world-hashes.sh generate` 22); with `-O1 -g` (assertions still on) run 45162258612 took 21 minutes (build 11.5, `ctest` 8.1, `world-hashes.sh generate` 1.5). `-O1` changes no world byte: all sixteen cases of `world-hashes.sh expect` match the golden files with this preset.

### Determinism lint

`tools/lint/determinism_lint.py` (owned by the determinism auditor; see `tools/lint/README.md`) reports constructs banned in the output path. Python 3 only, no build needed. From the repository root:

```
python3 -B tools/lint/determinism_lint.py --self-test
python3 -B tools/lint/determinism_lint.py
```

The second command scans `worldgen/` without `worldgen/tests/` and `tools/worldgen-cli/` without its `tests/` directory (`report` and `hash` text is part of the cross-platform contract), the project's CMake files for floating-point flags, and the pinned toml++ configuration. Exit status 1 means findings. CI runs both. On Windows use `py -3 -B`. The rule list and the suppression syntax are in `tools/lint/README.md`.

### Cross-compiler world hashes

`tools/ci/world-hashes.sh` (POSIX shell; Git Bash on Windows) holds the case list of spec P0-04 section 9 (the `empty-*` cases with `--until none`, amendment 16 (d)) and the geography cases of spec P1-00 section 14 X5 (`geo-seed-0-512`, `geo-seed-42-512`, `geo-phrase-512`, `geo-seed-1-1024`) and the history cases of spec P3-03 (`hist-span-2000-512`, `hist-races-elf-orc-1024`) and P3-04 (`hist-dense-1024`) and P5-02 (`p5-monsters-high-1024`: seed 7, `--monster-pressure high`), and is what CI runs. Every case reads the checkout's `game/data` through `--data`. For one built preset it generates the sixteen cases and saves, per case, `<preset>-<case>.db`, `<preset>-<case>.txt` (`hash --db <file> --detail`) and `<preset>-<case>.report.txt` (`report --db <file>`), and for every case whose world has a history (every case except `empty-*` and `pattern-*`; spec P3-07 amendment 18.6) also `<preset>-<case>.history.summary.txt` and `<preset>-<case>.history.full.txt` (`report --db <file> --history --level summary`, `--level full`) and `<preset>-<case>.territory.txt` (spec P4-01 13.6: lines `<year> <sha256>`, the SHA-256 of the PNG of `render --composite territory` at year 0, floor(E/2) and the history's end year E, ascending, each year once; the PNGs are rendered into a scratch directory beside the results and deleted), into `out/<source dir name>/world-hashes/` under the build root (`out/world-hashes/` when `UNMAPPED_BUILD_ROOT` is set). It refuses an output directory inside the source tree.

```
tools/ci/world-hashes.sh local linux-gcc-release linux-clang-debug   # generate for each built preset, then compare
tools/ci/world-hashes.sh generate linux-gcc-release                  # one preset
tools/ci/world-hashes.sh expect linux-gcc-release                    # hash --expect against the golden files
tools/ci/world-hashes.sh compare linux-gcc-release linux-clang-debug # compare saved results
tools/ci/world-hashes.sh cases
```

Each preset must be built first (default build tree, or `--build-dir DIR` for `generate` and `expect`). `compare` fails when any preset's text for any case (detail lines, world report and, for a case with history, both history reports and the territory hashes) differs from the others byte for byte or is missing, names the case, the file kind (`txt`, `report.txt`, `history.summary.txt`, `history.full.txt`, `territory.txt`) and the presets of each variant, and prints a `diff -u` of at most 200 lines per differing file (then the number of lines left out), so the differing table, layer or chronicle line is visible. Line ends are not normalised: a CR written on Windows is a difference. Generating all sixteen cases takes about a minute with a release preset (62 s with `linux-gcc-release`, measured 2026-11-22 at generator 0.20.0 on a shared machine; the cases run every implemented stage, history included); `local` with `linux-gcc-release` and `linux-clang-debug` took about ten minutes alongside a `ctest` run. The history reports add well under a second per case. The earlier figures of about two minutes with a debug preset and six with `linux-clang-asan` predate the history stages and are now low. It is not part of `ctest`, since one build tree cannot compare compilers.

### Golden values

- `worldgen/tests/golden/core_vectors.txt`: reference outputs of the deterministic core (spec P0-01 13.8), read by `worldgen-tests`.
- `worldgen/tests/golden/p0-worlds.txt`: SHA-256 of the world file for each P0 case, lines `<case> <hex>` sorted by case name, LF. Read by `worldgen-cli-tests` (skipped while the file is absent) and by `world-hashes.sh expect` in CI. Recorded by the validation specialist. Since the `worldgen` kind is registered, these worlds carry the content hash of the shipped `game/data`, so a change to the shipped tuning changes them too (P1-00 6.4).
- `worldgen/tests/golden/p1-worlds.txt`: the same for the geography cases of P1-00 14 X5, read by `world-hashes.sh expect` (a note, not a failure, while the file is absent). Recorded by the validation specialist.
- `worldgen/tests/golden/p3-worlds.txt`: the same for the history cases, read by `world-hashes.sh expect`. Recorded by the validation specialist.
- `worldgen/tests/golden/p1-01-elevation.txt`, `p1-02-hydrology.txt`, `p1-03-climate.txt`, `p1-04-biomes.txt`, `p1-05-resources.txt`, `p1-06-regions.txt`, `p3-01-data.txt` (content hashes of the three data roots, from `p3_data_reference.py`), `p3-02-names.txt` (keyed draws and names, from `p3_names_reference.py`), `p3-03-chronology.txt` (the chronology tables), `p3-04-engine.txt` (schedules and history tables, from `p3_engine_reference.py`), `p3-05-peoples.txt` (peoples histories, from `p3_peoples_reference.py`), `p3-06-conflict.txt` (conflict histories, from `p3_conflict_reference.py`), `p3-07-report.txt` (the history report: `<case> <level> <sha256>` for the world cases with history, the fixtures and `sample-42-1024`, plus each world case's `[stats]` lines; from `p3_report_reference.py`): each stage's golden vectors (layer and table hashes of fixture and shipped worlds), written by the test writer's independent reference in `worldgen/tests/reference/` and read by `worldgen-tests` and `worldgen-cli-tests` (the large cases by the `*-slow` suites). Never regenerated to make a C++ test pass.

Golden values describe the generator's output contract. A change that alters the bytes of any world file or golden vector must bump `GENERATOR_VERSION` (`worldgen/include/worldgen/version.hpp`) and re-record the golden values in the same change; the comparison across presets must pass before they are recorded. A schema change also needs a schema version bump, a spec update and a proposals entry (hard rule 7). Golden files are LF on every platform (`.gitattributes`).

### Choosing another build root

Presets cannot read an optional environment variable with a fallback, so `UNMAPPED_BUILD_ROOT` is applied with `-B` and the build and test steps name the directory:

```
export UNMAPPED_BUILD_ROOT=/some/fast/disk/the-unmapped
cmake --preset linux-gcc-debug -B "$UNMAPPED_BUILD_ROOT/build/linux-gcc-debug"
cmake --build "$UNMAPPED_BUILD_ROOT/build/linux-gcc-debug"
ctest --preset linux-gcc-debug --test-dir "$UNMAPPED_BUILD_ROOT/build/linux-gcc-debug"
```

Configure warns when `UNMAPPED_BUILD_ROOT` is set but the build directory is not under it. Continuous integration uses this form.

### Several worktrees

The default build tree includes the source directory name, so the main checkout and each git worktree get their own build trees and CMake caches. Two checkouts with the same directory name would collide; give one of them its own `UNMAPPED_BUILD_ROOT` with the `-B` form above.

### Compiler flags

All flags are set in `cmake/UnmappedCompilerFlags.cmake`:

- No fast-math on any compiler, and floating-point contraction off (`-fno-fast-math -ffp-contract=off` on GCC and Clang, `/fp:precise` on MSVC), for every target including vendored code. Never add `-ffast-math`, `-Ofast`, `/fp:fast` or `/fp:contract`.
- Project targets call `unmapped_project_target()`: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion` and more, or `/W4`, as errors. `-DUNMAPPED_WARNINGS_AS_ERRORS=OFF` turns off the error part for local experiments only.
- Vendored targets call `unmapped_vendored_target()`: warnings suppressed. Their headers are `SYSTEM` includes.
- SQLite compile options are set in one place only: the block marked `SQLITE CONFIGURATION` in `extern/CMakeLists.txt`. Runtime settings (page size, pragmas) belong in one place in `worldgen/src/db/`.
- ccache is used automatically when installed (`-DUNMAPPED_USE_CCACHE=OFF` to disable).

### Adding code

- Library sources go in `worldgen/CMakeLists.txt`, library tests in `worldgen/tests/CMakeLists.txt` (`worldgen-tests`), CLI sources in `tools/worldgen-cli/CMakeLists.txt` and CLI integration tests in `tools/worldgen-cli/tests/CMakeLists.txt` (`worldgen-cli-tests`). A new test file runs in the `-other` CTest entry; a large one gets its own `SHARD` line in the `unmapped_add_doctest_shards()` call at the end of the same file (pattern `*<file name>`, no directory). Every new project target calls `unmapped_project_target()`.
- Vendored libraries are linked as `unmapped::sqlite3`, `unmapped::doctest`, `unmapped::tomlplusplus` and `unmapped::stb_image_write`; with `UNMAPPED_BUILD_GDEXT=ON` also `unmapped::godot-cpp`.
- Extension sources go in `gdext/CMakeLists.txt` (`unmapped_gdext`); scenes and resources in `game/`. See `gdext/README.md`.

### Godot extension and viewer

Pinned versions, in summary. The record, with the evidence for the choice, is `gdext/README.md` ("Pinned versions"); the editor's SHA-512 digests are kept only in `tools/ci/godot-4.7.2-stable.sha512`:

| What | Version |
| --- | --- |
| Godot | **4.7.2-stable**, the newest stable Godot that Terrain3D supports: Terrain3D v1.1.0-stable (newest release, 2026-10-30) states "Supports Godot 4.5-4.8+", and Godot 4.8 is still in beta (checked 2026-11-15) |
| godot-cpp | **10.0.0-stable**, vendored in `extern/godot-cpp/`, built for GDExtension API **4.7** |

The extension is the CMake option `UNMAPPED_BUILD_GDEXT`, OFF by default; with it off nothing in the build changes. It builds with every preset except `linux-clang-asan`, where configure refuses it (a sanitized library cannot be loaded into the stock editor).

Install the pinned editor once with the fetch script (never done by the build; CI runs the same script):

```
tools/ci/fetch-godot.sh                       # Linux; on Windows run it from Git Bash
tools/ci/fetch-godot.sh --tools-dir DIR       # somewhere else; then configure with -DUNMAPPED_GODOT=<printed path>
```

It downloads the editor archive from the Godot release on GitHub, checks it and every extracted file against the SHA-512 digests in `tools/ci/godot-4.7.2-stable.sha512` (the archive digests are those of the release's `SHA512-SUMS.txt`), and installs into `~/.cache/the-unmapped/tools/godot-4.7.2-stable/` (`%LOCALAPPDATA%\the-unmapped\tools\godot-4.7.2-stable\` on Windows, with both `Godot_v4.7.2-stable_win64.exe` and the `_console.exe` the check runs), the folder CMake searches. It prints the editor's path. When the files are already there and match, it does nothing; when an installed file does not match, it stops and touches nothing. Needs curl, and unzip or Python 3. It ignores `UNMAPPED_BUILD_ROOT`, since CMake's lookup does.

Changing the Godot version means changing, together: the table above, `VERSION` in `tools/ci/fetch-godot.sh`, the digests file (renamed for the version), `UNMAPPED_GODOT_VERSION` and the editor names in `gdext/CMakeLists.txt`, and `gdext/README.md`.

Build the extension and run every test, including the headless check:

```
cmake --preset linux-gcc-debug -DUNMAPPED_BUILD_GDEXT=ON
cmake --build --preset linux-gcc-debug
ctest --preset linux-gcc-debug
ctest --preset linux-gcc-debug -R gdext-headless-check -V    # the headless check alone
```

`gdext-headless-check` is registered only when the pinned editor is found (in the tools folder above, on `PATH`, or named by `-DUNMAPPED_GODOT=<path>`). It generates `--seed 42 --test-pattern` and a real `--seed 42 --map-size 512` world, imports the project headless, runs the real viewer scene on each world and checks that it lists every layer (three, and the real world's layers from `report`, which must include `biome`, `elevation` and `rainfall`) and the composites `map`, `biomes` and `regions`, and shows, for each, the same pixels as `worldgen-cli render`. It takes about 7 seconds. A clean build with the extension takes about 3.5 minutes on 6 cores with an empty ccache (godot-cpp is about 1,100 files; `linux-gcc-release`, measured 2026-11-15), against under a minute without it; with a warm ccache a clean build of the same tree takes about 3 seconds.

Open the viewer on a test world (Linux; the path after `--` must be absolute):

```
B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-debug
GODOT=~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64
"$B/tools/worldgen-cli/worldgen-cli" generate --seed 42 --test-pattern --out ~/.cache/the-unmapped/out/seed42.db
"$GODOT" --path "$B/game" -- ~/.cache/the-unmapped/out/seed42.db
```

`"$GODOT" --editor --path "$B/game"` opens the project in the editor. Never open `game/` itself: Godot always runs on the run copy `<build>/game/` (see below).

### Continuous integration

`.github/workflows/ci.yml` runs on pushes to `main`, on pull requests and by hand (`workflow_dispatch`):

- `determinism-lint` (one Linux job): the two lint commands above.
- `build` (one job per preset, all seven, on `ubuntu-latest` and `windows-latest`, 180-minute job timeout): configure, build and `ctest` (four entries at a time, from the test preset; the hosted Linux and Windows runners have 4 vCPUs for a public repository) under `$RUNNER_TEMP/unmapped`; on `linux-gcc-release` and `windows-msvc-release` only, then the slow entries (`ctest --preset <preset>-slow`); then `world-hashes.sh generate` and `expect` (a warning, not a failure, while `p0-worlds.txt` is absent); upload the case files (every `<preset>-*` file: worlds, detail lines, world reports and history reports) as artifact `world-hashes-<preset>`; finally fail if the checkout holds any generated file.
- `compare-hashes` (Linux, after every `build` job, also when some failed, not when the run is cancelled): downloads every `world-hashes-*` artifact and runs `world-hashes.sh compare` over every build preset in `CMakePresets.json`, which compares the detail lines, the world report, the two history reports and the territory hashes of every case. A difference, or a preset with no artifact or a missing file, fails it. This job is the P0 exit check: the empty worlds are byte-identical across GCC, Clang and MSVC.
- `gdext` (two jobs, `gdext-linux` with `linux-gcc-release` and `gdext-windows` with `windows-msvc-release`): the extension. Fetches the pinned editor with `tools/ci/fetch-godot.sh --tools-dir "$RUNNER_TEMP/unmapped/tools"` (cached per Godot version and digests file; a restored copy is re-verified), configures with `-DUNMAPPED_BUILD_GDEXT=ON -DUNMAPPED_GODOT=<editor>` into `build/<preset>-gdext`, fails straight after configure if `ctest -N` does not list `gdext-headless-check`, builds, runs `ctest`, prints the headless check's full log, then `world-hashes.sh generate` and `expect` under the label `<preset>-gdext` (artifact `gdext-world-hashes-<preset>`, a name `compare-hashes` does not download), and ends with the clean-checkout check. On failure it also lists the run copy of the project and uploads the headless check's files.
- `compare-gdext-hashes` (Linux, after `build` and `gdext`): `world-hashes.sh compare <preset> <preset>-gdext` for both presets (the same files, history reports included), which proves that building `worldgen` position independent for the extension changes no output byte. A separate job, so that `compare-hashes` does not wait for the extension builds.

Compiler cache: `gdext-linux` uses ccache (cache key: preset, GCC version, hash of `extern/godot-cpp/`; saved once per key), so a run with an unchanged godot-cpp mostly hits. `gdext-windows` uses none, and configures with `-DUNMAPPED_USE_CCACHE=OFF`: neither ccache nor sccache has been tried with this build (godot-cpp adds a public `/MP` flag to every MSVC compile), and a cache that cannot be checked here is a risk the job does not need. The release preset has no debug information, so `/Zi` against `/Z7` is not what rules it out. A cold Windows extension build is expected to take 10 to 20 minutes. The seven `build` jobs use no compiler cache.

Third-party actions are pinned to full commit SHAs.

### Build output lives outside the source tree

Paul's checkout is inside a Dropbox folder, so nothing generated may be written into the repository directory. This is a requirement on the presets, not a habit to remember:

- Every CMake preset sets `binaryDir` outside the tree. The default root is `$HOME/.cache/the-unmapped/` (`%LOCALAPPDATA%\the-unmapped\` on Windows), overridable with the `UNMAPPED_BUILD_ROOT` environment variable through `-B` (see "Choosing another build root" above). Layout under the root: `build/<source dir name>/<preset>/` for build trees (see "Several worktrees" above), `out/` for generated worlds, map images, reports and seed-sweep results (`worldgen-cli`'s default; `world-hashes.sh` uses `out/<source dir name>/world-hashes/`), `tools/` for downloaded tools such as the pinned Godot editor.
- The top-level `CMakeLists.txt` refuses a build directory that is the source tree or inside it. CMake still writes `CMakeCache.txt` and `CMakeFiles/` into the refused directory before stopping; delete them.
- `worldgen-cli`, tests and sweep scripts take explicit output paths and default to `out/` under the build root, never the current directory.
- Compiled GDExtension libraries are produced under the build root and placed where the Godot project loads them by a symlink or by a path in the `.gdextension` file, not by copying binaries into `game/`. The build does this by assembling a run copy of the project in the build tree, `<build>/game/`: one link per entry of `game/`, a real `bin/` that is the extension's output directory, and Godot's own `.godot/` cache. Godot is always run on that copy, so neither the library nor the cache is written into the repository (`gdext/README.md`).
- `.gitignore` covers the usual in-tree build directories and Godot's `.godot/` cache as a backstop only.

Anything meant to be kept in the repository (golden hashes, review images attached to `docs/reviews/`) is copied in deliberately and kept small.
