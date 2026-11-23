# P4-03 History binding, year bar and territory in the viewer

Status: **r4, 2026-11-23** (r1 draft 2026-11-22, r2 and r3 2026-11-23; r4 the territory colour and border follow-ups of `docs/specs/p4-03a-territory-colour-followups.md`; revision notes at the end), for approval by the lead. Phase P4 Viewer v1, stage 3 of the overview `docs/specs/p4-00-viewer-v1-overview.md` (3.1 "P4-03", 3.2, rules V1 to V10, 6.3, 7, 8, 10). Builds on P4-01 r2 (`docs/specs/p4-01-timeline.md`: `worldgen/history/timeline.hpp`, `worldgen/inspect/territory.hpp`), P4-02 r2 (`docs/specs/p4-02-layer-catalogue.md`, the library catalogue the viewer already uses), the P4-01 audit (`docs/reviews/p4-01-timeline-audit.md`, advisories A2 and A3), P0-04 amendment 18 (e) (the `regions` composite and its border rule) and P2 as documented in `gdext/README.md`. Design doc sections 10 ("World viewer") and 13 (P4 row: "timeline scrubber, territory overlay by year"). Paul's decisions of 2026-11-22 (proposals, "the P4 overview's questions go with their recommendations"): the viewer opens at the present, year E; one hue family per race, shades per civilization.

Numbers quoted for the 512 world are from `worldgen-cli generate --seed 42 --map-size 512 --data game/data` at generator 0.21.0 (the headless check's real world, its `history.txt` from the viewer part's check run): E = 6000, 11 civilizations of which 1 ended, ages starting at 0, 2074, 3242, 4243, 4969 and 5596, 32 wars, 10 ruined settlements, the first `region_ceded` event in year 2483, 14 of 15 regions held at E. (At 0.20.0: 10 civilizations of which 2 ended, 30 wars, 15 ruined settlements, 13 regions held at E; E, the age starts and F were the same.) Timings quoted from P4-01 (3.1, 3.4, 8.1) are as measured at 0.20.0.

**No world byte changes.** P4-03 itself changes no world byte; its base is generator **0.21.0** (P5-01 merges with this stage), schema stays **7**. The library changes of section 9 change the pixels of `render --composite territory` and the golden file `worldgen/tests/golden/p4-01-timeline.txt` (section 9.6), nothing else that is recorded. The r4 changes (P4-03a) likewise change no world byte and leave `GENERATOR_VERSION` as it is on their base (0.22.0, P4-03a section 6).

## 1. Purpose

After this stage Paul opens a world with history and sees it at the present: a `territory` view, first in the list, with every held region in its holder's colour, a year bar under the map, and a legend of the realms alive in that year grouped by race. Dragging the slider or stepping by event or by change of holder redraws the realms as they grow and shrink; hovering a region names it and says who holds it and since when. A world without history opens as today, with the year bar disabled.

The stage also settles what P4-01 left interim in the territory image: the civilization colours (section 7) and realm borders (section 8), and takes in the two library advisories of the P4-01 audit (section 9).

## 2. What this stage touches

| Area | Files | Owner |
| --- | --- | --- |
| Binding and model | `gdext/src/world_file.*` (reader accessor), `gdext/src/viewer/viewer_model.*`, new `gdext/src/viewer/history_state.{hpp,cpp}` (C++ only, not registered) | viewer engineer |
| Screen | `gdext/src/viewer/viewer_screen.*`, new `gdext/src/viewer/year_marks.{hpp,cpp}` (Control `YearMarks`), `gdext/src/register_types.cpp`, `game/viewer/viewer.tscn` (layout only) | viewer engineer |
| Headless check | `gdext/src/viewer/viewer_check.*`, `gdext/cmake/headless_check.cmake` | viewer engineer |
| Library | `worldgen/include/worldgen/inspect/territory.hpp`, `worldgen/src/inspect/territory.cpp`, `worldgen/src/inspect/render.cpp`, `worldgen/src/inspect/render_internal.hpp`, `worldgen/src/history/timeline.cpp`, `worldgen/include/worldgen/history/digest.hpp`, `worldgen/src/history/digest_tables.cpp`, `worldgen/src/inspect/history_report.cpp` (call only), `tools/worldgen-cli/cli.cpp` (call only) | generator engineer |
| Library tests, reference, golden | `worldgen/tests/timeline_*.cpp`, `worldgen/tests/reference/p4_timeline_reference.py`, `worldgen/tests/golden/p4-01-timeline.txt` | test writer |
| Docs | `gdext/README.md` (viewer engineer); `docs/specs/p4-01-timeline.md` gets a pointer line to this spec (spec writer, with this spec) | |

Not touched: every world-writing path, `world-hashes.sh` (its `territory.txt` changes value, not form), the `Composite` enum and `ALL_COMPOSITES` (P4-01 5.4), `report` and `hash` text.

Order of work: the library part (section 9) is one branch, merged first (generator engineer, test writer, determinism auditor, validation specialist), since the viewer's pixel comparison needs the final image. The viewer part follows on its own branch. The viewer engineer may start the binding and the year bar against the P4-01 library at once; only the territory pixels and the legend colours wait for the library merge.

## 3. Binding

### 3.1 Where history is read

The history is read **once, when a world opens**, inside `ViewerModel::open_world`, after `WorldFile::open` succeeds and before `world_opened` is emitted:

1. `WorldFile` gains a C++-only accessor `WorldFile::reader()`, `const worldgen::db::WorldReader* reader() const noexcept` (null when closed). No SQL is added to the extension (V3).
2. The model calls `worldgen::history::load_history(*reader)` (P4-01 5.2) and keeps the result in a `HistoryState` (section 3.3).
3. Outcome, by the returned value:
   - success: history status `loaded`; year Y = E;
   - error `history_missing`: status `none`. The world opens exactly as in P2 (no `territory` view, year bar disabled, cell descriptions unchanged);
   - any other error, `out_of_memory` included: status `failed`, with worldgen's code name and message kept. The geography views open as in P2; no `territory` view; the year bar is disabled; the file label shows the failure (section 4.6). The world still counts as opened (`world_opened` is emitted, not `world_open_failed`), per V10.
4. `civilization_colours(tables)` (section 7) is computed once and kept. A FatalError from it (a civilization whose race has no row: impossible on a file `load_history` accepted) is caught and turns the status to `failed` with code `corrupt_layer` and the FatalError's text.
5. `TerritoryBase::build` is **not** run here. It runs on the first request of the territory image (in practice at once, since the world opens on that view), from the decoded layers `elevation`, `water`, `river`, `flow_dir`, `biome`, `region` and the world's table `biome`, the same layers the `regions` composite uses. If the layers fail `composite_regions_problem`, the territory view fails as any composite fails today (`view_failed` with worldgen's text); the year bar and the hover text keep working.

`close_world` and opening another world drop the whole `HistoryState`.

**Budget** (P4-00 8): opening a 1024 world with history may take at most 0.5 s more than opening it without (2 s at 2048), measured from `open_world` to the first frame showing the territory view. Expected: `load_history` under 20 ms, `TerritoryBase::build` 46 ms (P4-01 6.2), the six layer decodes the `regions` composite needs anyway.

### 3.2 What the model exposes to Godot

New methods and signals of `ViewerModel` (bound with `ClassDB::bind_method`, as the existing ones). Years and ids are `int64`. Every method is valid without history and answers as stated.

| Method | Returns |
| --- | --- |
| `get_history_status()` | `"none"`, `"loaded"` or `"failed"` (`"none"` with no world open) |
| `has_history()` | status is `loaded` |
| `get_history_error_code()`, `get_history_error_message()` | worldgen's code name and message when `failed`; empty otherwise |
| `get_end_year()` | E, or −1 without history |
| `get_year()` | Y, or −1 without history |
| `set_year(y)` | false and nothing changes if there is no history or y is outside [0, E]; true otherwise. If y ≠ Y: Y becomes y, the year state is recomputed (3.4) and `year_changed(y)` is emitted once. If y = Y: true, no signal |
| `step_year(delta)` | `set_year(clamp(Y + delta, 0, E))`; false without history |
| `step_event(direction)` | direction −1: `set_year(previous_event_year(Y))`; +1: `set_year(next_event_year(Y))` (P4-01 5.1: events by start year); false when there is none or no history. Any other direction: false |
| `step_holder_change(direction)` | −1: the largest value of `holder_change_years()` below Y; +1: the smallest above Y; then `set_year`; false when there is none |
| `has_previous_event()`, `has_next_event()`, `has_previous_change()`, `has_next_change()` | whether `step_event(−1)`, `step_event(+1)`, `step_holder_change(−1)`, `step_holder_change(+1)` would move; false without history |
| `get_year_info()` | Dictionary, section 3.5; empty without history |
| `get_year_marks()` | Array of Dictionary `{year, kind}`, section 5.4; empty without history |
| `get_region_holding(region)` | Dictionary `{region, civilization, name, since, event}` at Y (P4-01 `region_holder`; `name` the civilization's name, empty when free); empty for a region the timeline does not have, or without history |
| `get_civilization_info(civ)` | Dictionary `{id, name, race, race_key, race_name, race_plural, colour, founded_year, ended_year}`; `colour` 0xRRGGBB (section 7); `ended_year` −1 while it has none; empty for an unknown id |
| `get_civilizations_alive()` | `PackedInt64Array`, the civilizations alive at Y, ascending id (P4-01 `YearState::civilizations`) |
| `get_territory_view()` | the view index of `territory`, or −1 |

Signals added: `year_changed(year: int)`; `view_image_changed(index: int)` (the image of view `index` was rebuilt in place; its size is unchanged).

No method throws into Godot. A FatalError from the library for a year or id (P4-01 5.1) cannot be reached through these methods, since each checks its argument first; if one is reached all the same it is caught, logged with `push_error`, and the method returns false or empty.

### 3.3 `HistoryState` (C++ only)

Owned by the model, in `gdext/src/viewer/history_state.hpp`. Holds: the `LoadedHistory` (tables, view, timeline), the colour vector, the `TerritoryBase` once built, the current Y, the reused `YearState` (filled with the reusing overload `year_state(Y, out)`), the current holder vector (section 6.2), and the holder vector the territory image was last drawn with (r4: no fill vector; the image is drawn with the timeline form of 9.2). It has no Godot types except where it hands an `Image` to the model. Nothing in it is cached across a change of Y except the drawn holder vector, which only decides whether the image needs rebuilding (V6).

### 3.4 One year change

`set_year(y)` with y ≠ Y, in order, synchronously:

1. Y = y; `timeline.year_state(Y, state)`.
2. holders = `territory_holders(timeline, Y)` (section 9.2).
3. If the territory view's image exists and holders differ from the drawn holder vector: `render_territory(base, timeline, colours, Y, 1, buffer)` into the existing `Image`'s buffer (the timeline form of 9.2), drawn vector = holders, emit `view_image_changed(territory view)`. If the image does not exist yet (the view was never shown or failed), nothing is drawn now; it is drawn on its next selection.
4. If the selected view is `territory`, its legend is rebuilt (section 6.4).
5. Emit `year_changed(Y)`.

Holders change about 50 times in 6,000 years, so dragging the slider over quiet years rebuilds nothing; a rebuild is one `render_territory` (3.9 ms at 1024, P4-01 6.2) plus the texture update on the screen side.

**Budgets** (P4-00 8): steps 1 and 2 within 2 ms at 1024 (8 ms at 2048); step 3 within 40 ms (160 ms); a year change to the next drawn frame within 50 ms while dragging (200 ms). Debug builds may take four times as long. Measured with `--bench` (section 11.2).

### 3.5 `get_year_info()`

Dictionary, every value from the timeline and the tables, nothing restated:

| Key | Value |
| --- | --- |
| `year`, `end_year` | Y, E |
| `present` | Y = E |
| `age`, `age_name` | `YearState::age` and the `name` of that `age` row (empty for 0) |
| `phase`, `phase_kind`, `phase_start`, `phase_end` | `YearState::phase`; `phase_kind_word(kind)` of that `phase` row (section 9.5: `opening`, `quiet`, `flare_up`, `final`); its `start_year`, `end_year`. For phase 0 (at E): 0, `""`, −1, −1 |
| `realms` | number of civilizations alive at Y |
| `regions_held` | number of regions with a holder at Y |
| `settlements` | number of settlements that exist at Y with status 1 or 2 (`SettlementStatus::thriving`, `SettlementStatus::declining`, `worldgen/history/engine.hpp`) |
| `wars` | number of relations in force at Y whose `kind` is `war` |
| `events` | number of events of Y (`YearState::events`) |
| `events_starting` | number of events starting in Y |

## 4. The year bar

### 4.1 Layout (`game/viewer/viewer.tscn`, layout only)

A `VBoxContainer` `YearBar` is inserted in `Split/MapArea` between `MapRow` and `CellLabel`. Nodes, all with unique names:

```
YearBar (VBoxContainer)
  YearRow (HBoxContainer)
    %YearBack25      Button "-25"
    %YearBack1       Button "-1"
    %YearPrevEvent   Button "< event"
    %YearPrevChange  Button "< change"
    YearTrack (VBoxContainer, size_flags_horizontal expand)
      %YearSlider    HSlider (min 0, step 1, rounded)
      %YearMarks     YearMarks (custom Control, min height 10)
    %YearNextChange  Button "change >"
    %YearNextEvent   Button "event >"
    %YearForward1    Button "+1"
    %YearForward25   Button "+25"
    %YearField       SpinBox (min 0, step 1, rounded, editable)
  %YearLabel         Label (one line, overrun trimmed)
  %YearCounts        Label (one line, overrun trimmed)
```

Tooltips on the buttons name the action and the key (4.3). The scene holds no logic (V1); `ViewerScreen` finds the nodes with `require<T>()` as today and wires them.

### 4.2 Behaviour

- On `world_opened`: with history, the slider's and the field's max become E, their value Y (= E), every control is enabled; without history, every control in `YearRow` is disabled, the slider and field show 0 with max 0, and the labels show the texts of 4.4.
- Moving the slider (drag, click on the track, mouse wheel over it, which Godot's `HSlider` turns into steps of 1) calls `model.set_year(value)` on every `value_changed`. Typing a year in the field and pressing Enter, or using its arrows, calls `set_year` (Godot clamps the field to [0, E]).
- The buttons call `step_year(−25)`, `step_year(−1)`, `step_event(−1)`, `step_holder_change(−1)`, `step_holder_change(+1)`, `step_event(+1)`, `step_year(+1)`, `step_year(+25)`.
- On `year_changed(Y)` the screen sets the slider and the field to Y **without** re-emitting (`set_value_no_signal`), rewrites the labels, disables `< event` / `< change` when there is no earlier event year or change year and `event >` / `change >` when there is no later one (from `has_previous_event()`, `has_next_event()`, `has_previous_change()`, `has_next_change()`, 3.2), disables `-1`/`-25` at 0 and `+1`/`+25` at E, and refreshes the cell description of the cell last hovered (its text depends on Y).
- On `view_image_changed(i)` for the selected view, the map view updates its texture from the same `Image` (`ImageTexture::update`), keeping pan and zoom.

### 4.3 Keyboard

Handled in `ViewerScreen::_unhandled_key_input`, so a focused text field (the year field, the file dialog) keeps its keys; `accept_event` after a handled key.

**Focus rule.** Godot's focus navigation takes the arrow keys (`ui_left`, `ui_right`) before `_unhandled_key_input` whenever a focusable Control has keyboard focus. So the year buttons, `%YearSlider` and the map view (`%MapView`) take no focus (`focus_mode` none in `viewer.tscn`; the legend list already takes none), and the arrow keys reach `_unhandled_key_input` while none of the focusable controls holds focus. The layer list and the meta tree keep their focus behaviour: while one of them has focus (after a click on it) the arrow keys act on that control, not on the year; the year field likewise keeps its keys while focused. The other keys of the table are not taken by focus navigation.

| Key | Action |
| --- | --- |
| Left / Right | `step_year(−1)` / `step_year(+1)` |
| Shift+Left / Shift+Right | `step_year(−25)` / `step_year(+25)` |
| Page Up / Page Down | `step_event(−1)` / `step_event(+1)` |
| `[` / `]` | `step_holder_change(−1)` / `step_holder_change(+1)` |
| Home / End | `set_year(0)` / `set_year(E)` |

Without history these keys do nothing.

### 4.4 Readout texts (exact; the headless check compares them)

`%YearLabel`, with history:

```
Year <Y> of <E>[ (the present)]   |   <age name>   |   phase p<id> <kind word>, <start>-<end>
```

`(the present)` only when Y = E. The age part is the `YearState::age` of the timeline, left out when age is 0. At E the timeline gives the **last age** (P4-01 section 4 puts the present year in the last age), so the age part is present at E although the report's `[ages]` line of the last age ends at E − 1. At E (phase 0) the phase part reads `no phase`. Example at E for seed 42 at 512: `Year 6000 of 6000 (the present)   |   The Age of Orcs   |   no phase`. Separators are three spaces, `|`, three spaces, as in the cell description. Example: `Year 2477 of 6000   |   The Age of Goblins   |   phase p5 flare_up, 2474-2523`.

`%YearCounts`, with history:

```
<realms> realm[s] alive, <held> region[s] held, <settlements> settlement[s] standing, <wars> war[s], <events> event[s] this year
```

singular exactly when the number is 1.

Without history: `%YearLabel` is `No history in this world.` and `%YearCounts` empty. Status `failed`: `%YearLabel` is `History not read: <code>: <message>` (worldgen's code name and message as kept by `HistoryState`, 3.1 step 3; `corrupt_layer` with the FatalError's text for a failure of `civilization_colours`, 3.1 step 4) and `%YearCounts` empty; the file label gains its line of 4.6.

### 4.5 Marks under the slider

`YearMarks` (new `Control`, registered in `register_types.cpp`) draws, for each entry of `model.get_year_marks()`, a vertical line at x = (width − 1) · year / E (floats: presentation only, V4): `awakening` the full height, `phase` half the height, `present` the full height at the right end. Its tooltip at the mouse is the nearest mark's text (`awakening of the <race plural>, <year>`, `phase p<id> <kind word>, <start>-<end>`, `the present, <E>`). Without history it draws nothing. Holder-change years are not marked (they are reached with `[` and `]`).

### 4.6 File label on a history failure

`<path>` as today, then a new line `history: <code>: <message>` (V10). With status `none` or `loaded` the file label is as today.

### 4.7 Command line (arguments after `--`)

- `--year Y`: after the world opens, `set_year(Y)`. With no history, or Y not a decimal integer in [0, E], the status label shows `--year <text>: not a year of this world (0 to <E>)` (or `... this world has no history`) as an error and the viewer stays at E.
- `--bench`: section 11.2 (prints `UNMAPPED-BENCH:` lines and quits).
- Check options: section 10.

## 5. Views and lists

### 5.1 The `territory` view

A world with history status `loaded` whose `available_composites` include `regions` gets one more composite view, **`territory`**, listed **first** under the Composites heading (before `map`), so the world opens on it (P4-00 10 step 1; the P2 rule "a world opens on the first entry" is unchanged). `get_view_info` gives kind `composite`, name `territory`, group `Composites`, width and height of `elevation`, element type and palette `composite`. `get_composite_count()` counts it. The composites `map`, `biomes` and `regions` follow as today.

Model-side, the view list keeps `Composite` values for the three geography composites and marks the territory view separately (it is not a `Composite` enumerator, P4-01 5.4).

Its image is the RGB8 `Image` of `render_territory(base, timeline, colours, Y, 1)` at the current Y: the same bytes as `worldgen-cli render --composite territory --year Y --scale 1` (V4). It is created on first selection and then redrawn in place (3.4). `get_cell_text` for it is `elevation <value>`, as for every composite.

### 5.2 Without history

No `territory` view; views, legends, descriptions and the image of every other view exactly as in P2 and P4-02.

### 5.3 Selection and year

A year change never changes the selected view. Selecting `territory` after year changes draws it at the current Y if the drawn holder vector differs (or it was never drawn).

### 5.4 `get_year_marks()`

In ascending year, ties in this order: one `awakening` per present race with an `awakening_year` (from `HistoryTables::races`), one `phase` per `phase` row at its `start_year`, and one `present` at E. Dictionary `{year, kind, label}`, `label` the tooltip text of 4.5.

## 6. Territory: hover and legend

### 6.1 Hover text

`describe_cell(x, y)` keeps its P2/P4-02 text and, when the history status is `loaded` and the cell's `region` value v > 0 is a region of the timeline (`region_ids()`), appends, for the holding h of v at Y:

| Holding | Appended |
| --- | --- |
| held | `   |   held by <civilization name> since <h.since>` |
| free, `since` > 0 | `   |   free since <h.since>` |
| free, `since` = 0 | `   |   free` |

This applies in every view (the year is the screen's state), not only in `territory`. A cell with v = 0 (`no region (ocean)`) or a region the timeline lacks gets nothing appended. Example at Y = 6000: `x 211, y 260   elevation 312.4 m   |   biome Temperate forest   |   region 11 Gull's Pit   |   held by The House of Finen since 0`.

Civilization names are the `name` column as stored (as `HistoryView` gives them), unescaped.

### 6.2 Holders and fill

The viewer never decides who holds what: holders come from `territory_holders` and colours from `territory_fill`, both library functions of the timeline (V3).

### 6.3 Legend title

`territory: holders in year <Y>; free land as in regions`

### 6.4 Legend rows (`get_legend()` while `territory` is selected), in this order

1. For each race with at least one civilization alive at Y, in ascending race id: a heading row, label `<race plural>` (the `race` row's `plural`), colours empty; then each civilization of that race alive at Y, ascending id: label `<civilization name>`, colours `[colour]` (section 7).
2. `border between holders`, colours `[0x000000]`, tooltip `black, between regions of different holders`.
3. `border inside a realm`, colours empty: no swatch, since the colour depends on the realm (section 8: dim of the realm's fill); tooltip `the realm's colour, darker`.
4. `border in free land`, colours empty: no swatch, since the colour depends on the land (8.2, half); tooltip `the land's colour, darker`.

The three border labels are short so that they fit the legend's width; the longer explanation is the row's tooltip (`get_legend()` row key `tooltip`, empty on every other row; the list shows it on hover).
5. A heading row `free land`, colours empty; then the rows of `composite_legend(Composite::regions, biomes)` after its `region border` row and before its last three rows, in its order (the biome rows of land biomes).
6. A heading row `water`, colours empty; then the last three rows of that legend, in its order: the water rows W of P4-02 4.4 (`ocean, 0 to 3000 m deep`, `lake`, `river, class 1 to 6+`). The ocean is not free land.

The screen shows a row with empty colours as a non-selectable heading with no swatch. The legend is rebuilt on every year change while `territory` is selected (step 4 of 3.4).

## 7. Civilization colours (replaces P4-01 5.5 for the six shipped race keys)

### 7.1 Problems of the interim table

Measured with the distance of 7.3 against the background set B of 7.3 (water, rivers, lakes and the free-land biome colours drawn beside held land): the interim human shades are 20, 2 and 11 from water colours (human base (50, 105, 200) against river class 2 (55, 110, 210); human dark (31, 65, 125) against the ocean ramp at about 50 m depth); elf dark (32, 93, 43) is 26 from free temperate forest (46, 96, 52); goblin light (222, 207, 152) is 7 from steppe (224, 210, 150). Across races, dwarf deep (70, 35, 19) and orc deep (87, 17, 15) are 44 apart. The arithmetic shade rule darkens towards black and brown and lightens towards grey, which is where the biome colours are, so no choice of six bases fixes it.

### 7.2 New table

Each shipped race gets **four explicit colours**, saturated, so that held land reads as political colour against the muted biome palette. A civilization of a shipped race with ordinal k (P4-01 5.5: civilizations of the same race with a smaller id) takes entry k mod 4. Humans move from blue to **teal**: every saturated blue lies within 80 of a river colour (section 12, question 1).

| Key | k mod 4 = 0 | 1 | 2 | 3 | min to B | min within race |
| --- | --- | --- | --- | --- | --- | --- |
| `elf` (green) | (40, 190, 70) | (130, 215, 130) | (0, 140, 40) | (130, 205, 40) | 109 | 126 |
| `goblin` (yellow) | (230, 195, 20) | (235, 215, 90) | (170, 125, 0) | (140, 155, 30) | 89 | 89 |
| `dwarf` (orange-brown) | (220, 105, 25) | (245, 165, 105) | (150, 65, 15) | (195, 80, 55) | 93 | 78 |
| `faerie` (violet) | (175, 70, 225) | (215, 160, 250) | (115, 35, 165) | (200, 110, 200) | 127 | 97 |
| `human` (teal) | (0, 190, 195) | (100, 215, 215) | (0, 120, 130) | (50, 215, 175) | 96 | 94 |
| `orc` (red) | (205, 30, 30) | (240, 120, 110) | (135, 15, 20) | (230, 50, 80) | 165 | 93 |

This is the **one place** that holds the 24 values (r4; P4-03a). r4 changed four entries: goblin 3 (205, 160, 60) to olive (140, 155, 30), a hue apart from goblin 0 and 2 (critic finding 1); goblin 1 (250, 235, 110), elf 1 (150, 230, 150) and human 1 (125, 235, 230) darkened by about 15 per cent by the test of 7.5 (critic finding 2). Entry 0 is the race's base colour: `race_base_colour(key)` returns it for these six keys. The closest pair across races is dwarf 1 / orc 1 at 90; the closest pair of one race is dwarf 0 / dwarf 3 at 78 (goblin 2 / goblin 3: 89).

**Any other key** keeps P4-01 5.5 unchanged: the base from the key's hash (`TAG_COLOUR`), the shade by `civilization_shade(base, k)` (base, light, dark, deep). Such colours carry no distance guarantee; a race added by P5 gets its own row here when it ships.

### 7.3 The distance rule (testable, integer)

For colours a = (R1, G1, B1), b = (R2, G2, B2), with r̄ = floor((R1 + R2) / 2):

D(a, b) = (512 + r̄)(R1 − R2)² + 1024 (G1 − G2)² + (767 − r̄)(B1 − B2)²

(the "red-mean" weighted RGB distance, scaled by 256 so that it is an integer; distance d ≥ T means D ≥ 256 T²; the table's "min" columns give floor(sqrt(floor(D / 256)))). Colours compared unshaded (hillshade factor 256); the shading of P1-00 10.2 multiplies both sides of a border alike.

Background set B, the colours the territory image draws next to held land:

1. `ramp_colour(HYPSOMETRIC_OCEAN_STOPS, t)` for every t in 0 … 30000;
2. the lake colour (64, 128, 204);
3. `composite_river_colour(k)` for k = 1 … 6;
4. the `colour` of every row of table `biome` of a world generated from the shipped `game/data` (all twelve rows, water rows included; the table depends only on the data).

Required for the 24 entries of 7.2:

| Rule | Threshold |
| --- | --- |
| C1 every entry against every colour of B | D ≥ 256 · 80² (1,638,400) |
| C2 two entries of one race | D ≥ 256 · 70² (1,254,400) |
| C3 two entries of different races | D ≥ 256 · 80² |

The table passes with the margins shown (worst: goblin 1 against steppe, 89; within a race, dwarf 0 / dwarf 3, 78). A change to the shipped biome colours that breaks C1 fails the test, which is intended: the two palettes are judged together.

### 7.4 API

`territory.hpp` gains:

```cpp
// The colour of the civilization with ordinal k (>= 0) of race `race_key`: entry k mod 4 of 7.2 for
// the six shipped keys, else civilization_shade(race_base_colour(key), k). FatalError if k < 0.
[[nodiscard]] Rgb race_civilization_colour(std::string_view race_key, std::int64_t ordinal);
```

`civilization_colours` uses it. `race_base_colour` returns entry 0 for the six keys and the hash base otherwise; `civilization_shade` is unchanged (it is now used only for keys without a row).

### 7.5 Luma and pale entries (r4)

Luma of a colour c = (R, G, B), integer: **L(c) = floor((77 R + 150 G + 29 B) / 256)**.

It is used twice: by the inner-border dim of 8.2 (bound L > 175) and, as a design test only, to pick the entries of 7.2 that were too pale (L > 190 on the r3 table: goblin 1 225, human 1 201, elf 1 196, each darkened by about 15 per cent in r4; the test chooses what to change and is not required of the result). L of the r4 table:

| Key | 0 | 1 | 2 | 3 |
| --- | --- | --- | --- | --- |
| `elf` | 131 | 179 | 86 | 163 |
| `goblin` | 185 | 206 | 124 | 136 |
| `dwarf` | 130 | 182 | 84 | 111 |
| `faerie` | 119 | 186 | 73 | 147 |
| `human` | 133 | 180 | 85 | 160 |
| `orc` | 82 | 154 | 51 | 107 |

## 8. Realm borders (decided: drawn)

### 8.1 Decision and reasons

The `regions` composite draws every region border black (P0-04 amendment 18 (e)), so in P4-01's territory image every realm is cut into its regions by black lines and a realm's outline is not visible as such. This stage keeps black **only where the holders differ** and draws a border inside one realm in a darkened shade of the realm's colour:

- The realm reads as one shape at a glance, which is the point of the view.
- It is what separates two neighbouring realms whose colours are close or equal: shades of one race after a secession, and the fifth and later civilizations of one race, which reuse colours (k mod 4).
- It adds no line width and no new pixel position: only the colour of cells that are already border cells changes. `render_territory` with an empty holder vector still equals the `regions` composite (P4-00 6.3).
- r4 (critic finding 3): a border with both sides free is drawn at half strength, half the land colour, so that the free land is not a web of black lines as strong as the realm outlines; black is kept wherever a holder is on at least one side. P4-00 6.3's "free land shows the `regions` composite unchanged" now holds for the fill of free land; its land borders are softened when holders are given.
- Cost: `TerritoryBase` keeps two more `uint16` region values and a kind byte per cell (12 bytes a cell instead of 8: 12 MB at 1024, 48 MB at 2048); `render_territory` does at most two more holder lookups on border cells (a few per cent of cells). Expected well under 1 ms at 1024 on top of 3.9 ms.

### 8.2 The rule, pixel by pixel (extends P4-01 5.6)

Holders: `holders[v]` is the civilization holding region v at Y, 0 when free, or when v ≥ `holders.size()`.

A cell drawn by steps 3 or 4 of the composite map, or with e < 0, is neither fillable nor a candidate: it is drawn as B (kind 0 of 9.2, with region, east and south 0).

A cell (x, y) with region value v, elevation e, is an **inner border candidate** iff all of: v > 0; e ≥ 0; it is not drawn by steps 3 or 4 of the composite map (the same painted flag as "fillable"); and it **is** a border cell of amendment 18 (e) (its E neighbour or its S neighbour has a region w > 0, w ≠ v). Its **other regions** O are the E neighbour's value when that neighbour makes it a border cell, and the S neighbour's value when that one does (one or two values).

Pixel of a candidate, in this order (r4):

1. `holders` is empty: B(x, y), which is black. (An empty vector means no holder information; the image is P4-01's, and with an empty fill the `regions` composite.)
2. `holders[v]` = c ≠ 0, `holders[w]` = c for every w in O, v < `fill.size()` and `fill[v]` holds colour C (the fill of the cell's own region v; the regions of O are compared by holder only, their fill is not used): `shade_colour(dim(C), factor)`, with factor the cell's `hillshade_factor` and dim(C) = (floor(R · n / 16), floor(G · n / 16), floor(B · n / 16)), **n = 10 when L(C) > 175, n = 12 otherwise** (L of 7.5, of the unshaded C). On r4's table n = 10 for goblin 1, faerie 1, goblin 0, dwarf 1, human 1 and elf 1, and n = 12 for the other 18 entries; keys without a row follow the same test.
3. `holders[v]` = 0 and `holders[w]` = 0 for every w in O (both sides free; `holders[u]` is 0 for u ≥ `holders.size()`): **half(U)** = (floor(U.r · 8 / 16), floor(U.g · 8 / 16), floor(U.b · 8 / 16)), where U = `shade_colour(biome_colour(biome table, biome(x, y)), factor)` is the cell's unbordered colour, the pixel the cell has in the `biomes` composite (step 2 of the composite map; shaded first, then halved). fill is not consulted.
4. Otherwise (a holder on at least one side but not the same on all, or c ≠ 0 with `fill[v]` missing): B(x, y), black.

A border cell of kind 0 (a lake cell, a river-painted cell, or e < 0) stays black in every case. A half-strength cell is never next to held land along an edge: a free cell whose E or S neighbour is held falls under 4, and a held W or N neighbour is itself a black border cell. So C1 of 7.3 needs no half-strength colours in B.

Every other cell is as P4-01 5.6 (fillable cells filled, the rest B).

Example (4 × 2, no water layers, elevation 10 everywhere, factor 256): `region` row 0 `1 2 3 3`, row 1 `1 2 3 3`; regions 1 and 2 held by an elf civilization with ordinal 0, region 3 by a second elf civilization (ordinal 1). Cells (0, 0) and (0, 1): border cells (E neighbour 2), both sides held by the same civilization: dim((40, 190, 70)) = (30, 142, 52). Cells (1, 0) and (1, 1): E neighbour 3 held by another civilization: black. Cells (2, ·) and (3, ·): (130, 215, 130) (r4 elf 1; cell 2 has E neighbour 3 = v, not a border). With the same layers, region 3 free and regions 1 and 2 held as before: cells (1, ·) black (one side held). With every region free (holders all 0) and biome colour (224, 210, 150) (steppe): cells (0, ·) and (1, ·) half((224, 210, 150)) = (112, 105, 75); with holders empty they are black. An inner border of a goblin civilization of ordinal 1 (L 206): dim((235, 215, 90)) = (146, 134, 56) (n = 10).

## 9. Library items

All in `worldgen`, hard rule 1 and V5, tested headless by the test writer from this spec.

### 9.1 Colours

Section 7.4: `race_civilization_colour`, the table of 7.2, `civilization_colours` through it.

### 9.2 Territory with holders and inner borders

```cpp
namespace worldgen::inspect {
// Index region id; the same size and id rules as territory_fill (P4-01 5.3); entry the civilization
// holding the region at `year`, 0 when free; [0] = 0. FatalError for a year outside 0..E with the
// timeline's text.
[[nodiscard]] std::vector<std::int64_t> territory_holders(const history::Timeline&, std::int64_t year);

class TerritoryBase {
public:
    // build: unchanged signature and errors.
    struct Cell {
        Rgb base;                  // B(x, y)
        Rgb half;                  // r4: kind 2: half(U) of 8.2 rule 3; else (0, 0, 0)
        std::uint8_t kind = 0;     // 0 drawn as B only (v = 0, e < 0, or drawn by step 3 or 4), 1 fillable,
                                   // 2 inner border candidate (8.2)
        std::uint16_t factor = 0;  // hillshade factor (160..352) when kind is 1 or 2, else 0
        std::uint16_t region = 0;  // v when kind is 1 or 2, else 0
        std::uint16_t east = 0;    // kind 2: the E neighbour's value when it makes the cell a border, else 0
        std::uint16_t south = 0;   // kind 2: the S neighbour's value when it makes the cell a border, else 0
    };
    // width(), height(), cells(): unchanged.
};

// The territory image (P4-01 5.6 with 8.2). holders may be empty (then no inner border is drawn).
// FatalError: scale outside 1..16, size overflow, out.size() != territory_rgb_size(base, scale).
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase&, std::span<const std::optional<Rgb>> fill,
                                                         std::span<const std::int64_t> holders, std::uint32_t scale);
void render_territory(const TerritoryBase&, std::span<const std::optional<Rgb>> fill,
                      std::span<const std::int64_t> holders, std::uint32_t scale, std::span<std::uint8_t> out);

// r4 (audit A1): fill and holders from one timeline and year, so that they cannot disagree. Equal to
// render_territory(base, territory_fill(timeline, colours, year), territory_holders(timeline, year),
// scale[, out]), with the same FatalErrors in the same order (year, missing colour, then scale and size).
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase&, const history::Timeline& timeline,
                                                         std::span<const Rgb> colours, std::int64_t year,
                                                         std::uint32_t scale);
void render_territory(const TerritoryBase&, const history::Timeline& timeline, std::span<const Rgb> colours,
                      std::int64_t year, std::uint32_t scale, std::span<std::uint8_t> out);
}
```

The two-vector overloads stay (tests and hand-made holders use them); the CLI and the viewer use the timeline form (r4). `Cell` is 16 bytes with `half` (16 MB at 1024, 64 MB at 2048); it is never hashed or serialized. `TerritoryBase::build` sets `half` from the composite's own unbordered colour of the cell (the shaded colour before the border pass blackens it), not by re-deriving it.

The P4-01 overloads without `holders` are **removed** (one behaviour, one function); `worldgen-cli render --composite territory` calls the timeline form (r4; r1 to r3: the two-vector form with `territory_holders(timeline, Y)`). The CLI's options, messages, exit codes and help text are unchanged (P0-04 amendment 23 stands; the help line "the regions composite with each region held in the year in the colour of its holder" remains true).

`TerritoryBase::build` takes kind, east and south from the composite's own border pass (`detail::regions_composite_cells` gains the two neighbour values per border cell; the black rule itself and every `regions` pixel are unchanged).

### 9.3 A2: one layout loop

`fill_buffer` (render.cpp, the loop that writes each cell's colour into a scale × scale block) moves to `render_internal.hpp` as `detail::fill_buffer`, unchanged; `render_rgb`, the composites and `render_territory` all call it. No pixel of any image changes; the existing render, composite and territory tests are the check.

### 9.4 A3: `Timeline::build` checks control runs itself

A region's `region_control` rows must be contiguous and the runs in region order; `Timeline::build` already refuses otherwise with `timeline: region_control row of region <r> out of order` (P4-01), before the checks below.

Rows are then visited in table order. For each row r: first r's own check, if r has a `to_year`: `to_year` > `from_year`; then, if the previous row p is of the same region, the check of p against r: p's `to_year` is not NULL and p.`to_year` ≤ r.`from_year`. The first failure raises FatalError:

- `timeline: region_control rows of region <r> overlap at year <r.from_year>` (p open, or p.to_year > r.from_year; this also covers from_year not ascending, since then p.to_year > p.from_year ≥ r.from_year);
- `timeline: region_control row (<r>, <from>) ends at <to>, not after it starts`.

Example: p = (2, 300, 1400), r = (2, 1300, 1300) (region, from_year, to_year). p passes its own check; at r, r's own check comes first and reports `timeline: region_control row (2, 1300) ends at 1300, not after it starts`, although p.to_year > r.from_year as well.

Equal boundaries (p.`to_year` = r.`from_year`, a region changing hands in one year) are accepted. On every file `load_history` accepts these never fire (`read_history_tables` enforces the same, P3-07 5.3), so `load_history` behaviour and every golden line are unchanged; the check makes `holder_at`'s precondition local for callers that build a `Timeline` from hand-made tables.

### 9.5 Phase kind words

`digest.hpp` gains `[[nodiscard]] std::string_view phase_kind_word(std::int64_t kind) noexcept;`: 1 `opening`, 2 `quiet`, 3 `flare_up`, 4 `final` (`PhaseKind::opening`, `quiet`, `flare`, `final` of `worldgen/history/schedule.hpp`), anything else empty. It is the report's `PHASE_KIND` table moved; `history_report.cpp` calls it and still prints `?` for a kind whose word is empty, so `report --history` bytes are unchanged (P3-07 golden `p3-07-report.txt` must still match). The viewer shows phase kinds only through it (V3).

### 9.6 What has to be re-recorded

Sections 7 and 8 change territory pixels and civilization colours; no world byte. **P4-03 does not bump `GENERATOR_VERSION`**: on this base it is 0.21.0, set by P5-01, which merges with this stage (the rule of CLAUDE.md "Golden values" applies to world files and golden vectors of the generator's output; these are inspection outputs, and the P4-01 golden file is re-recorded in the same change as its own rule says).

| File or output | Changes | Re-recorded by |
| --- | --- | --- |
| `worldgen/tests/reference/p4_timeline_reference.py` | table 7.2, `race_civilization_colour`, `territory_holders`, rule 8.2 in its territory renderer, the distance rule 7.3 as a check, the A3 texts | test writer |
| `worldgen/tests/golden/p4-01-timeline.txt` | every `<case> static` line whose world or fixture has a civilization of a shipped race (its `colour` lines change); every `<case> territory <Y>` line. Recorded on the 0.21.0 base, so the world cases' `year` and `years` lines also change (0.21.0 worlds), and each fixture's `table meta` line; the P3-07 fixture `h11-v2` is added (39 lines). The rule "`year` and `years` must not change" holds for the rule change on the same inputs only: the reference's `--rules p4-01` mode on the 0.20.0 inputs confirms it, and the test writer records that diff in the commit message | test writer, `python3 -B worldgen/tests/reference/p4_timeline_reference.py golden`; never edited to make a C++ test pass |
| C++ unit tests of P4-01 13.2 items 1, 2, 7 | expected colours and pixels written in the tests | test writer |
| `render --composite territory` PNGs; CI `<preset>-<case>.territory.txt` of `world-hashes.sh` | values change; no golden file holds them, so nothing is recorded; `compare` must still pass across presets | validation specialist confirms |
| `p0-worlds.txt`, `p1-worlds.txt`, `p3-worlds.txt`, every other golden file | unchanged; any difference is a defect | validation specialist confirms |
| P4-01 spec | pointer line "Section 5.5 for the six shipped race keys and the image of 5.6 at region borders are amended by P4-03 sections 7 and 8; render_territory takes holders (P4-03 9.2)" at its status | spec writer, with this spec |

## 10. The headless check

Everything it checks today stays, with the same messages, except where a count or list grows by `territory` (stated below). New world and new reference files are produced by `headless_check.cmake`; new checks run in `viewer_check.cpp` through the real scene (V9). The CTest entry stays one entry, under 60 s on `linux-gcc-release`, 300 s timeout.

### 10.1 Worlds and references (`headless_check.cmake`)

1. Test pattern: as today. Viewer run adds `--expect-history none`.
2. **New: geography-only world**: `worldgen-cli generate --seed 42 --map-size 512 --data <repo>/game/data --until regions` into `<work>/geo/`; its layers listed and rendered as for the real world, composites `map`, `biomes`, `regions`. Viewer run with `--expect-composites 3 --expect-history none`.
3. Real world (`--seed 42 --map-size 512`), as today, plus:
   - `worldgen-cli report --db <world> --history --level full --out <real>/history.txt`;
   - from it (cmake string parsing): E from the `[stats]` line `end_year = <E>` only (`[settings]` has `history_span`, not `end_year`); the age start years from the `[ages]` lines `a<id> <name>: <start>-<end>, …`; the year F of the first chronicle line matching `^([0-9]+)(-[0-9]+)? e[0-9]+ region_ceded/`;
   - **world content asserts** (P4-00 7), each a FATAL_ERROR naming the missing feature and saying "choose another seed and record it in gdext/README.md": `[stats]` `events_region_ceded` ≥ 1, `wars` ≥ 1, `settlements_ruined` ≥ 1, `civilizations_ended` ≥ 1;
   - the year list L = {0, every age start, F − 1, F, floor(E / 2), E − 1, E}, ascending, each once;
   - `render --composite territory --out <real>/composite.territory.png` (default year E: the existing per-view loop compares the view at the opening year with it), and for each Y in L `render --composite territory --year Y --out <real>/territory-<Y>.png`;
   - the viewer run adds `--expect-composites 4` (was 3), `--expect-history loaded`, `--expect-end-year <E>`, `--expect-years <L comma-separated>`, `--history-report <real>/history.txt`.

Seed 42 at 512 gives L = {0, 2074, 2482, 2483, 3000, 3242, 4243, 4969, 5596, 5999, 6000} at 0.21.0, as at 0.20.0 (11 renders, about half a second).

### 10.2 New checks in the viewer (`viewer_check.cpp`)

With `--expect-history none` (pattern and geography-only worlds):

- N1 `get_history_status()` is `none`; no view is named `territory`; `get_territory_view()` is −1.
- N2 every control of `YearRow` is disabled; `%YearLabel` is `No history in this world.`; `%YearCounts` is empty; `set_year(0)` returns false; a Right key event sent to the screen leaves `get_year()` at −1.
- N3 the hover text of the existing hover check contains neither `held by` nor `free`.

With `--expect-history loaded` (real world):

- H1 status `loaded`; `get_end_year()` = `--expect-end-year`; the slider's min 0, max E; the world opened at Y = E (`get_year()`, slider value and field value all E); the selected view is `territory` and is the first entry under Composites.
- H2 For each Y in `--expect-years`, set through the slider (`%YearSlider.set_value(Y)`, so the screen's wiring is exercised): `get_year()` = Y; the map view's image equals `territory-<Y>.png` pixel for pixel (the existing `compare_images`); `%YearLabel` equals the text of 4.4 with the age name and phase taken from the report's `[ages]` and `[phases]` lines covering Y (age: the `[ages]` line with start ≤ Y ≤ end, and at Y = E the last `[ages]` line, 4.4; phase: the `p<id> a<age> <kind> <start>-<end>, …` line with start ≤ Y ≤ end; none at E); the territory legend's civilization rows are exactly the civilizations alive at Y according to the report's `[civilizations]` blocks (`c<id> <name>`, `  founded: <f> …`, `  fate: ended in <e>, …` or `  fate: alive in …`: alive iff f ≤ Y and not ended at or before Y), by name, in the order of 6.4, each under the heading of its race (`  race: <Name> (ra<id>)`).
- H3 At E: for every `[present]` line `c<id> <name>: <n> region[s], …`, the number of regions whose `get_region_holding` names civilization `<id>` equals n; the regions held by no civilization of `[present]` are free.
- H4 Steps, from Y = E: `step_event(+1)` is false; `step_event(−1)` gives the year of the last chronicle event line (the largest start year in `history.txt`); from Y = 0, `step_holder_change(+1)` gives the smallest year > 0 at which some region's `get_region_holding` differs from the year before (found by the check stepping `set_year` year by year from 0, through the model), and it is ≤ F, where the viewer check takes F itself from `--history-report`: the start year of the first chronicle line of kind `region_ceded` (the same line 10.1 reads); `step_holder_change(−1)` from F + 1 gives F; `step_year(+25)` from E − 10 gives E; `step_year(−25)` from 10 gives 0. Each step emitted `year_changed` exactly once (counted by a connected callable), and a `set_year` to the current year emitted none.
- H5 Keyboard: with no control focused (the check releases focus first, 4.3 focus rule), Right from E − 1 gives E; Shift+Left from E gives E − 25; Home gives 0; End gives E; Page Up from E gives the same year as H4's `step_event(−1)`.
- H6 Hover: at Y = E, the first cell in row-major order whose region is held (by `get_region_holding`) and the first whose region is free; each description ends with the text of 6.1 for that holding (civilization name from the report's `[civilizations]` block of that id; `since` as the model's holding); the same held cell at Y = 0 ends with the 6.1 text of its holding at 0. Then `hover_cell(−1, −1)` restores the prompt, as today.
- H7 Colours: for every civilization, `get_civilization_info(c).colour` equals the colour the library gives (`civilization_colours` of the check's own `load_history` of the same file; the check may call the library directly, P4-00 7: library correctness is doctest's job, the check proves the wiring); and every swatch of the territory legend equals its civilization's colour.
- H8 Year marks: `get_year_marks()` holds one `awakening` per age start year of `[ages]`, one `phase` per `[phases]` line at its start, and one `present` at E, in ascending year.
- H9 (r4) Territory legend tail: at E, the rows after the last civilization row are exactly, label and colours: `border between holders` [0x000000]; `border inside a realm` []; `border in free land` []; `free land` []; the rows of the `regions` view's legend between its `region border` row and its last three rows, in order; `water` []; the last three rows of the `regions` legend, in order. The three border rows carry the tooltips of 6.4, every other row of the tail an empty one.

The existing checks (every layer and composite pixel-equal to the CLI, legends, the biome rows of `biomes` and `regions`, the hover on the first composite entry, reader errors, units) run unchanged; on the real world the composite list now has four entries and the first-entry hover is on `territory`, whose text still contains everything the existing hover check asks for.

The check prints, as information only (P4-00 8), the open time, the time of the first territory image and the slowest of the H2 year changes.

### 10.3 Not covered by the check

A history that `load_history` refuses (status `failed`): no world in the check has one, and corrupting a table needs a tool the check does not have. The viewer engineer verifies it by hand once (a copy of the 512 world with one `region_control` row edited by `sqlite3`, under the build root) and records the screen texts in the validation note. Legibility of the colours and borders is Paul's walkthrough and the world critic's screenshot review (P4-00 10).

## 11. Tests, timings and invariants

### 11.1 Library tests (test writer, `worldgen-tests-timeline`; independent reference)

1. `race_civilization_colour` for the six keys, k = 0 … 7 (wrap), against 7.2 (r4 values); `race_base_colour` gives entry 0; `troll` and the empty key unchanged from P4-01 5.5; FatalError for k = −1.
2. Rules C1, C2, C3 of 7.3 over the 24 entries, B built as 7.3 (the biome rows read from a shipped-data world's table `biome`), with D computed in `int64`; r4: the two "min" columns of 7.2 reproduced exactly, and L of 7.5 for every entry, with L > 175 exactly for the six entries named in 8.2.
3. `civilization_colours` on the interleaved two-race fixture (new expected values).
4. `territory_holders` equals `YearState::regions` holders at every year of T1, with the size and id rules of `territory_fill`.
5. Inner borders on hand-made layers (T1 extended, at most 8 × 6): a border between two regions of one realm (dimmed, once with a realm colour of L > 175 at n = 10 and once with L ≤ 175 at n = 12), two realms (black), a realm and free land (black), two free regions (r4: half(U)), a free-free border cell that is also a lake cell or river-painted (black), a cell whose E neighbour is free and whose S neighbour is held (black), a cell whose E neighbour is of the same realm and whose S neighbour is of another (black), a border cell whose region is held but whose fill is missing (black); scales 1 and 2; expected pixels written in the test.
6. `render_territory(base, {}, {}, k)` equals `render_composite_regions` at k = 1, 3; r4: `render_territory(base, fill_with_all_free, holders_all_zero, k)` equals it except on kind-2 cells, which are half(U); `render_territory(base, fill, {}, k)` draws every border black (P4-01's image).
6a. r4: the timeline form equals the two-vector form with `territory_fill` and `territory_holders` of the same year, at every year of T1 and at the golden years of the 512 world cases, scale 1 and 3, both the vector and the `out` form; and gives the same FatalError text for a bad year, a short colour vector, a bad scale and a wrong `out` size.
7. A3: `Timeline::build` on hand-made `HistoryTables` with (a) one region's rows with `from_year` descending, (b) p.`to_year` > r.`from_year`, (c) p open followed by another row, (d) `to_year` = `from_year`: each FatalError text of 9.4; and no error for p.`to_year` = r.`from_year`.
8. `phase_kind_word` for 0 … 5; the P3-07 report goldens still pass (no new test needed).
9. Property (P4-01 13.5 item 8, revised invariant 9): every pixel of `render_territory` is B, a fill colour shaded on a kind-1 cell of a held region, a dimmed fill colour shaded on a kind-2 cell whose rule 8.2 item 2 holds, or (r4) half(U) on a kind-2 cell whose rule 8.2 item 3 holds; kind 2 exactly on border cells that would otherwise be fillable.
10. Golden: section 9.6.
11. Bench `timeline-bench`: `render_territory` with holders at 1024 seed 42 (prints only).

### 11.2 Viewer timings (viewer engineer, validation note)

`--bench` (after `--` with a world path; runs headless, no display needed): opens the world, prints the time of `open_world` and of the first territory image, then calls `set_year` for every Y from 0 to E and back, printing the slowest and the mean year change, the number of image rebuilds and the slowest rebuild, then quits with exit 0. Times from `Time::get_ticks_usec` (presentation side only; nothing reaches `worldgen`). The validation note records them for seed 42 at 1024 with `linux-gcc-release`, against 3.1 and 3.4; a budget missed by more than a factor of two is a defect (P4-00 8). Frame time while dragging is judged by hand in the walkthrough, with Godot's FPS monitor.

### 11.3 Invariants (viewer)

1. With history, the model's Y is always in [0, E] and the slider, the field and `%YearLabel` always show it after `year_changed`.
2. The territory image at Y is byte-equal to `worldgen-cli render --composite territory --year Y`.
3. The image is rebuilt only when the holder vector changes (count of rebuilds over 0 … E ≤ |`holder_change_years()`|).
4. Without history, every view, legend and description is as in P2/P4-02, and no year control acts.
5. Nothing in `gdext/` issues SQL on a history table or tests an event kind by id (V3, V7).

### 11.4 What the determinism auditor reviews

The library change only (the viewer adds nothing to `worldgen`): the constants of 7.2 and that `race_civilization_colour` compares keys as bytes; rule 8.2 in `TerritoryBase::build` and `render_territory` (the E/S neighbour values taken from the composite's own border pass, not re-derived; `holders` index bounds; integer `dim`); that `detail::fill_buffer` moved unchanged; the A3 checks' bounds and that they cannot fire on an accepted file; `phase_kind_word` moved with its strings unchanged; and that the golden diff touches only `static` and `territory` lines. Lint must stay clean. The distance rule 7.3 lives in tests and the reference only.

## 12. Open questions for Paul

1. **Humans in teal.** Every saturated blue is within 80 of a river colour, so humans move to teal (7.2). Recommendation: accept teal. If Paul wants blue humans, the alternative is to darken the river and lake colours, which changes every composite image and their golden vectors.
2. **Inner borders.** Borders inside one realm are drawn in the realm's colour dimmed to 3/4 (r4: 10/16 for colours with L > 175, P4-03a open point 1), so region outlines stay faintly visible (8). The other choice is to leave them out (each realm a flat patch; hover still names the region). Recommendation: the dimmed line.
3. **Saturated realm colours.** The new table is deliberately brighter than the biome palette, so held land reads as political colour; on a dark-fantasy map it may look loud. Recommendation: judge it in the walkthrough; if too loud, lower all 24 entries together while keeping rules C1 to C3, a change of golden values only.

## 13. Readings chosen

1. The hover holding text appears in every view while the world has history, not only in `territory` (6.1).
2. A year change that leaves the holders unchanged redraws nothing (3.4); the legend and labels are still rewritten.
3. The phase kind is shown as the report's word (`flare_up`), not a reworded label, so the viewer and the report agree (V3).
4. A history failure leaves the world open with its geography (V10); it is shown in the file label and in `%YearLabel`.
5. The headless check's year list replaces P4-00 7's "first year a region changes hands" by the year F of the first `region_ceded` event and F − 1, which `report --history` lets the check find, plus the model-side search of H4.

## Revision notes

r2 (2026-11-23, after library integration on `p4/03-int`; corrections to match the code, no change of behaviour):

1. 9.4: the check order is stated (rows in table order; for each row its own check first, then the check of the previous row of the same region against it), with an example; the contiguity and region-order precondition already enforced by `Timeline::build` (P4-01) is named.
2. Status line, 9.6: the base is generator 0.21.0 (P5-01 merges with this stage), not 0.20.0; P4-03 itself changes no world byte. Numbers quoted at 0.20.0 in the header and in 10.1 are left as measured.
3. 9.6: the re-recorded `p4-01-timeline.txt` also changes the world cases' `year` and `years` lines and each fixture's `table meta` line, and gains fixture `h11-v2` (39 lines); "year and years unchanged" holds for the rule change on the same inputs only (`--rules p4-01` on 0.20.0 inputs).
4. 8.2, 9.2: a cell drawn by step 3 or 4, or with e < 0, is kind 0 with region, east and south 0; the dimmed colour is `fill[v]` of the cell's own region.
5. 9.5: `history_report.cpp` still prints `?` for a kind whose `phase_kind_word` is empty.
6. 3.5, 9.5: the named constants `SettlementStatus` and `PhaseKind` are given beside the numbers.

r3 (2026-11-23, after the viewer part on `p4/03-viewer`; corrections to match the code, one legend text changed):

1. 4.4, H2: the age of the present is the last age, as the timeline gives it (P4-01 section 4); the readout at E reads e.g. `Year 6000 of 6000 (the present)   |   The Age of Orcs   |   no phase`, and H2 takes the last `[ages]` line at E (proposals, "Viewer v1, P4-03: the year readout at the present").
2. 10.1: E is read from the `[stats]` line `end_year` only; `[settings]` has `history_span`.
3. H4: the viewer check takes F from the report's chronicle, the first `region_ceded` line.
4. Header, 10.1: the 512 world's figures at 0.21.0 (11 civilizations, 1 ended, 32 wars, 10 ruined settlements, 14 of 15 regions held at E); E, the age starts, F and L unchanged from 0.20.0; P4-01 timings stay as measured at 0.20.0.
5. 4.3, H5: the focus rule: the year buttons, the slider and the map view take no focus, so the arrow keys reach `_unhandled_key_input`; while the layer list or the meta tree has focus the arrows act on it.
6. 4.4: the `failed` texts as built, `History not read: <code>: <message>` and the file label line `history: <code>: <message>` (4.6), with `corrupt_layer` for a failure of `civilization_colours`.
7. 3.1, 4.7: `WorldFile::reader()`, `--year Y` and `--bench` named as built (`HistoryState` and `YearMarks` were already named in 2, 3.3 and 4.5).
8. 6.4: the legend row reads `border within a realm (the realm's colour, darker)`, still without a swatch (a code change: the viewer currently writes `border within a realm`).

r4 (2026-11-23, the change spec `docs/specs/p4-03a-territory-colour-followups.md`: the world critic's findings 1 to 5 and the library audit's A1; the lead's decisions after Paul saw the images):

1. 7.2: goblin 3 to (140, 155, 30); goblin 1, elf 1, human 1 to (235, 215, 90), (130, 215, 130), (100, 215, 215); the min columns of goblin (89, 89) and human (96) updated; 7.3's worst margins restated. The table is the one place that holds the colours.
2. 7.5 new: the integer luma L and the r4 table's L values; the pale test L > 190 on the r3 table that chose the darkened entries.
3. 8.1, 8.2: a border with both sides free is half(U), half the cell's unbordered land colour; the inner-border dim is 10/16 when L(C) > 175, else 12/16; an empty holder vector still draws every border black; a worked example for each.
4. 9.2: `Cell::half`; the timeline form of `render_territory`, used by the CLI and the viewer (3.3, 3.4, 5.1); the two-vector forms stay.
5. 6.4: the free-land border row and a `water` heading before the water rows; 10.2 H9 checks the legend tail. Later in r4 (lead's ruling, the long rows were cut off at the legend's width): the border rows read `border between holders`, `border inside a realm`, `border in free land`, with the longer text as tooltips; H9 checks labels and tooltips.
6. 11.1 items 1, 2, 5, 6, 6a, 9: tests for the above.
