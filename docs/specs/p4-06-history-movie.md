# P4-06 History movie

Status: **r1 draft, 2026-11-23**, for approval by Paul or the lead. Phase P4 Viewer v1, stage 6 (the last) of the overview `docs/specs/p4-00-viewer-v1-overview.md` (3.1 "P4-06", 5 rules V1 to V10, 6.2, 7 row P4-06, 8, 10 step 6, 11 question 2). Builds on P4-03 r4 (year bar, `HistoryState`, `YearMarks`, the focus rule of 4.3), P4-04 r2 (event places and rings, `event_symbol`, `event_symbol_colour`), P4-05 r2 (browser, `BrowserState`, follow, the event list, B0 to B12) and the viewer as built on `origin/main` at 568df26 (generator 0.23.0, schema 9; `gdext/README.md`). Design doc 10 ("Viewer v1 decisions": "History movie"). Proposals read: "Viewer: a history movie on the 2D map" (Paul's request) and "Viewer v1: the history movie is playback, recorded with Godot's Movie Maker" (accepted 2026-11-22).

**No world byte changes, no library part.** Generator stays 0.23.0, schema 9; `report`, `hash`, every golden file and `world-hashes.sh` are unchanged. Everything the movie needs already exists in `worldgen` (section 2). Owner: viewer engineer; the world critic reviews a recording.

## 1. Purpose

After this stage the year bar plays. **Play** runs the years forward on the map at a chosen speed. The territory, the markers and the browser follow each year as when the slider is dragged. A **ticker** over the bottom of the map shows the sentences of each year as it is reached, and the places of those events **flash**. **Skip quiet years** passes years in which nothing is listed at ten times the speed. Playback walks the browser's current event list, so selecting a civilization and pressing Play plays that civilization's story from its first event to its last. Command-line options start playback over a span and quit at its end, so one command records a movie with Godot's Movie Maker mode. This is step 6 of the exit walkthrough (P4-00 10).

## 2. Parts, and why there is no library part

| Area | Files | Owner |
| --- | --- | --- |
| playback state (C++ only, no Godot types) | new `gdext/src/viewer/playback_state.hpp`, `.cpp` | viewer engineer |
| model | `viewer_model.hpp`, new `viewer_model_playback.cpp` | viewer engineer |
| screen, map view | `viewer_screen.hpp`, new `viewer_screen_playback.cpp`, `layer_map_view.*` (flashes) | viewer engineer |
| scene | `game/viewer/viewer.tscn` (play row, ticker; layout only, V1) | viewer engineer |
| check | new `viewer_check_playback.cpp`, `.hpp`; `gdext/cmake/headless_check.cmake` | viewer engineer |
| docs | `gdext/README.md` (recording a movie, keys, options, bench) | viewer engineer |

What playback reads, all through existing library calls (V3):

| Need | Source |
| --- | --- |
| the event list it plays, its years, levels, groups | `BrowserIndex::select`, `events()` (P4-05 4.5), through `BrowserState` |
| the ticker's text | the row texts `BrowserState::row` (worldgen's `row_text`) |
| years in which a region changes hands | `Timeline::holder_change_years()` |
| phase starts and their labels | `HistoryState::year_marks()` (kind `phase`) |
| the place and colour of a flash | `MarkerIndex::event_place`, `event_symbol(kind_key)`, `event_symbol_colour` (P4-04 3.4, P4-05 12.1 F5) |
| everything shown at a year | `ViewerModel::set_year` as today |

The timing rule (section 4) is presentation and lives in the viewer. It never reaches a world file, and the check tests it against its own copy of the rule (P3).

## 3. Definitions

- **Speeds** (years per second): the list `SPEEDS` = 1, 2, 5, 10, 20, 50, 100, 200, 500. Default **10**. Every value divides 1,000,000, so the dwell below is exact.
- **Dwell** D = 1,000,000 / S microseconds for speed S. **Quiet factor** Q = 10 (interim, question 1): a quiet year takes D / Q when skip quiet years is on.
- **Playback filter**: the browser's filter (P4-05 5.1: selection, scope, group, level), with its year span open (from = to = −1) and This year off. Without the browser available it is "every event". **L** is `BrowserIndex::select` of that filter: ascending event ids.
- **Span** [P0, P1], 0 ≤ P0 ≤ P1 ≤ E. Default: with scope `entity` or `place` and L not empty, P0 = the year of L's first event and P1 = the largest `year` in L; otherwise [0, E]. `play(from, to)` with −1 for either end takes that end of the default.
- **Stop years** S_stop: the years in [P0, P1] that are (a) the `year` of an event of L, or, with scope `all` only, (b) a holder-change year or (c) the start year of a phase; plus P0 and P1. The start year of a multi-year event is a stop, and its later years are not.
- **Quiet year**: a year of [P0, P1] not in S_stop.
- dwell(y) = D when y ∈ S_stop or skip is off; D / Q otherwise.

S_stop is recomputed when playback starts and whenever the selection, scope, group or level changes (P4-05 `browser_selection_changed` and the filter setters). It does not depend on Y.

## 4. Timing model

### 4.1 Clock

Playback advances only by `playback_advance(dt)`, dt in whole microseconds. On screen, `ViewerScreen::_process(delta)` calls it with dt = min(llround(delta × 1,000,000), 250,000) while the state is `playing` or `ended` (floats on the Godot side only, V4). Under Movie Maker and `--fixed-fps N`, Godot gives every frame the same delta 1/N, so dt is the same integer every frame (33,333 at 30) and the year shown in frame k is a function of the world, the options and k. No wall-clock time enters the model. The headless check calls `playback_advance` directly with fixed dt (a simulated clock).

### 4.2 The rule (normative; the check runs its own copy)

State: y (= Y, the year shown), r (microseconds already spent at y), clock (microseconds played this session).

```
play: y = start year (4.4); set_year(y) as playback; r = 0; arrive(y)

advance(dt):
  if state == ended: clock += dt; age flashes; return false
  if state != playing: return false
  clock += dt; r += dt; moved = false
  loop:
    if r < dwell(y): break
    if y == P1: state = ended; emit playback_finished; break
    r -= dwell(y); y += 1; moved = true
    if y in S_stop: r = 0; break        # arriving at a stop ends this frame's movement
  if moved: set_year(y) as playback; arrive(y)
  return moved

arrive(y): ticker and flashes of y (sections 6 and 7)
```

Consequences, which are requirements:

1. **At most one year change per frame**, however many years the clock covers. Quiet years crossed inside one frame are never drawn; each answer is a function of Y (V6), so the next drawn year is correct.
2. **No stop year is crossed.** Every stop year is the year shown for at least one frame and gets its ticker lines and flashes. On a busy stretch the movie therefore runs slower than S: at most one stop year per frame (30 a second at 30 fps). The leftover time on arrival at a stop is dropped.
3. A movie of span [P0, P1] lasts at least Σ dwell(y) and at least |S_stop| frames.
4. The implementation may jump over a run of quiet years arithmetically instead of year by year, provided the result equals the loop exactly.

### 4.3 Frame budget, and speeds beyond it

The year change dominates a frame. P4-05 measured, on `linux-gcc-release`, seed 42 at 1024, under `xvfb-run`: whole `set_year` at most 21 ms (mean 0.13), `set_year` to the next drawn frame at most 34 ms (mean 26), a frame without a year change 21 to 22 ms (the copy to the virtual screen). Rule 1 bounds playback at one such change per frame, so any speed up to 500 years a second costs the same per frame as dragging the slider. A frame that runs long gives a larger dt, clamped at 250 ms, so a stall never jumps more than a quarter second of play. The playback's own work per frame (advance, ticker, flash list) is budgeted at 1 ms (section 10). Movie Maker renders offline and has no time budget: a slow frame only makes the recording take longer.

### 4.4 States

`stopped` (no session), `playing`, `paused`, `ended`.

| From | Event | To |
| --- | --- | --- |
| `stopped` | `toggle_playback` / `play(from, to)` | `playing`; start year Y when P0 ≤ Y < P1, else P0 |
| `playing` | `toggle_playback` | `paused` |
| `paused` | `toggle_playback` | `playing`, from Y when P0 ≤ Y < P1, else from P0 (r = 0) |
| `playing`, `paused`, `ended` | a year change not made by playback (5.3) | `paused` at the new Y, r = 0 |
| `playing` | advance reaches the end of P1 | `ended`, `playback_finished` once |
| `ended` | `toggle_playback` | `playing` from P0 |
| any | `stop_playback`, world closed or opened | `stopped`, ticker and flashes cleared |

`play(from, to)` from any state starts a new session over that span.

## 5. Model (`PlaybackState`, `ViewerModel`)

### 5.1 `PlaybackState` (C++ only, no Godot types)

Owned by the model beside `HistoryState` and `BrowserState`. Holds the state, span, speed, skip flag, S_stop (sorted vector), y, r, clock, the ticker entries (6) and the flashes (7). Also a pure function for the command line (9.2):

```cpp
struct PlayOptions { bool play = false; std::int64_t from = -1, to = -1; std::int64_t speed = 10;
                     bool skip_quiet = true; bool quit_at_end = false; bool log = false; bool no_pane = false; };
// args: the user arguments after "--"; has_history, end_year: the open world's.
// Returns the options, or the first error text of 9.2.
std::variant<PlayOptions, std::string> parse_play_options(std::span<const std::string> args,
                                                          bool has_history, std::int64_t end_year);
```

### 5.2 Bound methods and signals

Every method is valid without history; it then answers empty, false or `stopped`.

| Method | Returns / does |
| --- | --- |
| `play(from, to)` | starts a session (4.4); −1 for a default end; false without history or for a span outside [0, E] or from > to |
| `toggle_playback()`, `stop_playback()` | 4.4; false without history |
| `playback_advance(usec)` | 4.2; true when the year changed |
| `get_playback()` | `{state, from, to, speed, skip_quiet, clock, stops}` (`stops` the size of S_stop) |
| `get_playback_speeds()` | PackedInt64Array of `SPEEDS` |
| `set_playback_speed(s)`, `step_playback_speed(d)` | s in `SPEEDS`, else false; d ±1 moves one place in the list, clamped. Takes effect at once; r is kept, so the current year's dwell continues under the new speed (no year change at that call) |
| `set_playback_skip_quiet(on)` | at once, as the speed |
| `get_playback_stop_years()` | PackedInt64Array of S_stop (for the check) |
| `get_ticker_lines()` | Array of `{year, event, kind, text, current}` oldest first (6.1) |
| `get_flashes()` | Array of `{event, x, y, colour, age, life}` (7) |

Signals: `playback_changed()` (state, span, speed or skip changed), `playback_finished()`, `ticker_changed()`.

### 5.3 Year changes from outside

`ViewerModel::set_year` marks a change made by `playback_advance` or `play`. Any other change of Y (slider, field, buttons, keys, `step_event` with or without follow, `select_event`, `browser_back` and `browser_forward` that restore a year, `--year`) while the state is `playing`, `paused` or `ended` pauses playback at the new Y (4.4), clears the flashes and refills the ticker with the lines of the new Y alone. A `set_year` to the year already shown changes nothing and does not pause.

### 5.4 Follow and the browser filter

- **Playback plays the browser's list.** L is the list the browser shows and follow walks, without the year span and This year (3). So the most wanted use, a civilization's story, is: choose the civilization in the list (P4-05 turns follow on and scope to `entity`), press Play. The span defaults to its first to last event, the stops are its events' years, and the ticker shows only its events. "A realm's wars" is the same with the kind group `war`. A place (settlement or region) plays its events likewise.
- With scope `all` the movie is the whole history at the browser's level, and holder changes and phase starts are stops as well (3).
- **Follow on or off does not change playback.** Follow is a stepping mode, and its steps during playback are year changes from outside (5.3). Playback never changes the selection: it selects no event, pushes no path and does not move the event list's selection. The event list scrolls to Y on each year change as today (P4-05 6.3).
- A selection or filter change during playback recomputes S_stop and continues from Y with the span unchanged; a new span needs a new `play`. The ticker keeps its lines.
- With **This year** on, the browser's own list changes every year; playback ignores This year (3).

## 6. Ticker

### 6.1 Entries (model)

On `arrive(y)` the model appends, in this order:

1. With scope `all`, when y is the start year of a phase: `{year y, event 0, kind "phase", text "<y>  <label>"}`, where `<label>` is the phase mark's label (P4-03 4.5: `phase p<id> <kind word>, <start>-<end>`).
2. For each event of L with `year` = y, ascending id: `{year y, event e, kind "event", text <row text of e>}` (P4-05 3: `<span>  <sentence>[ (<note>)]`).

The ticker holds at most **6** entries; older entries are dropped from the front. When y alone has more than 6, the entries of y are its first 5 followed by `{year y, event 0, kind "more", text "<y>  ... and <n> more"}`, n the number left out. `current` is true exactly for the entries of the year shown. A year with no entries leaves the ticker as it was. The ticker is cleared on `play` and `stop_playback`; on a year change from outside it holds the entries of the new Y only.

**Levels.** At summary (the browser's default) the ticker shows the summary set: at seed 42 at 1024 most stop years have one or two lines. At full ("All events") every event of the year, including the minor ones (settlement growth, quiet stretches); a year with more than six is cut as above. The level is the browser's, so the ticker always shows the lines of one of the two reports' chronicles.

### 6.2 Screen

`%Ticker`, a `PanelContainer` over the bottom of `%MapView` (its child, anchored to the bottom edge with 8 px margins, `mouse_filter` ignore on it and its children, so hover, click and drag reach the map). Background black at alpha 0.6; inside, a `VBoxContainer` `%TickerLines` with one `Label` per entry, newest at the bottom, autowrap on word boundaries. Entries of the year shown in white, older ones in (170, 170, 170). Its height is at most 40 % of the map view; when the wrapped entries do not fit, the oldest are hidden. It is visible when the ticker toggle is on (default on, key **T**), the state is not `stopped`, and it has entries. It is rebuilt only on `ticker_changed`.

## 7. Flashes

On `arrive(y)`, each event of L with `year` = y and a place (`event_place` ≠ none) adds a flash `{event, x, y, colour, age 0, life 1,200,000}`; colour = `event_symbol_colour(event_symbol(kind_key))` (white for plain). At most 32 flashes; the oldest is dropped. A flash's age grows with `clock` (while `playing`, and while `ended` so that the last ones fade out, 4.2) and it is removed when age ≥ life. Flashes are cleared on `play`, `stop_playback` and a year change from outside, and frozen while `paused`.

`LayerMapView` draws them last, above the markers and the P4-05 highlights: a ring (`draw_arc`, full circle) centred on the cell's screen centre, radius 6g + 14g × age / life screen pixels and width 2g, with g the glyph cell size of P4-05 12.1 F1, in the flash colour with alpha 1 − age / life. The screen hands `get_flashes()` to the map view and calls `queue_redraw` on every frame while one is alive. The P4-04 rings of the year's events are drawn as before.

## 8. Screen and keys

### 8.1 Year bar (replaces the layout of P4-03 4.1)

P4-05 Part B found that at 1280 × 800 the year row's nine controls leave the slider about 10 px. The slider moves to a row of its own, shared with the play controls:

```
YearBar (VBoxContainer)
  PlayRow (HBoxContainer)                     new
    %PlayButton      Button "Play" / "Pause"  (tooltip "Play or pause (Space); stop: Shift+Space")
    YearTrack        (moved here, expand)
      %YearSlider
      %YearMarks
    %PlaySpeed       OptionButton "1 y/s" ... "500 y/s"  (tooltip "Speed in years per second (- and =)")
    %PlaySkipQuiet   CheckBox "Skip quiet"    (tooltip "Pass years with nothing listed at ten times the speed (Q)")
  YearRow (HBoxContainer)
    %YearBack25 %YearBack1 %YearPrevEvent %YearPrevChange  (expanding spacer)  %YearNextChange %YearNextEvent %YearForward1 %YearForward25 %YearField
  %YearLabel
  %YearCounts
```

`%PlayButton`, `%PlaySpeed` and `%PlaySkipQuiet` take no keyboard focus (`focus_mode` none, P4-03 4.3), so Space, the arrows and the speed keys reach `_unhandled_key_input`. Without history the three are disabled. The play button reads `Pause` while playing and `Play` otherwise.

**Layout requirement** (checked by P10): with the right pane shown, at 1280 × 800 the slider is at least 240 px wide and no control of the year bar lies outside `MapArea`; at 1600 × 1000 the slider is at least 400 px. If `YearRow` is too wide at 1280 × 800, the engineer moves `%YearField` to the end of `PlayRow` and records it in `gdext/README.md`. The extra row costs the map about 31 px of height.

### 8.2 Keys (`_unhandled_key_input`, P4-03 4.3 focus rule)

| Key | Action |
| --- | --- |
| Space | `toggle_playback()` |
| Shift+Space | `stop_playback()` |
| `-` / `=` (and keypad `-` / `+`) | `step_playback_speed(−1)` / `(+1)` |
| Q | skip quiet years on / off |
| T | ticker shown / hidden |

The keys of P4-03 and P4-05 keep their meaning; those that change the year pause playback (5.3). Without history the five keys do nothing. While a list, the search field or a spin box has focus it keeps Space (a focused `ItemList` or `LineEdit` takes it); Escape releases the focus first (P4-05 6.5).

### 8.3 Window sizes

At 1600 × 1000 the map keeps about 800 × 830 px (31 px less than P4-05's 860), and the ticker covers at most its lower 40 %. At 1280 × 800 it is about 540 × 630. Key **B** (P4-05) or `--no-pane` gives the map the right pane's width, which is the recommended form for a recording (9.3).

## 9. Command line and recording

### 9.1 Options (after `--`, with a world path)

| Option | Meaning |
| --- | --- |
| `--play <from> <to>` | after the world opens and the start options (`--select`, `--browser-group`, `--browser-full`, `--zoom`, `--centre`) are applied, set Y to the span's start; 1.0 s later (process time) `play(from, to)`. Each value is a decimal year or `-` for the default end (3) |
| `--speed <s>` | the speed, one of `SPEEDS` (default 10) |
| `--skip-quiet on\|off` | default on |
| `--quit-at-end` | 2.0 s (process time) after `playback_finished`, quit with exit status 0 |
| `--play-log` | print `UNMAPPED-PLAY: <frame> <year>` on every year change (frame counted from `play`, 0 the start) and `UNMAPPED-PLAY: finished at <P1> after <frames> frames` at the end |
| `--no-pane` | start with the right pane hidden (as key B) |

With `--play`, `--year` is ignored (the span's start wins) and the status line shows `--year ignored: --play sets the start year`.

### 9.2 Refusals (exact texts; checked by P9)

The first that applies is shown in the status line as an error and printed with `printerr`; with `--quit-at-end` the viewer then quits with exit status 1, otherwise it stays open without playing.

| Case | Text |
| --- | --- |
| `--play` without two values | `--play needs two values: a year or - for each end` |
| a value neither a decimal integer nor `-` | `--play <from> <to>: not a year: <value>` |
| a span outside [0, E] or from > to (after defaults) | `--play <from> <to>: not a span of this world (0 to <E>)` |
| world without history | `--play: this world has no history` |
| speed not in `SPEEDS` | `--speed <s>: not one of 1, 2, 5, 10, 20, 50, 100, 200, 500` |
| bad skip value | `--skip-quiet <w>: on or off` |
| `--quit-at-end` or `--play-log` without `--play` | `--quit-at-end needs --play` / `--play-log needs --play` |
| with `--screenshot` or `--bench` | `--play cannot be combined with --screenshot or --bench` |

`--check` ignores the play options (the check drives playback itself).

### 9.3 Recording a movie (documented in `gdext/README.md`)

```
B=~/.cache/the-unmapped/build/$(basename "$PWD")/linux-gcc-release
GODOT=~/.cache/the-unmapped/tools/godot-4.7.2-stable/Godot_v4.7.2-stable_linux.x86_64
OUT=~/.cache/the-unmapped/out/movies; mkdir -p "$OUT"
xvfb-run -a -s "-screen 0 1600x1000x24" "$GODOT" --path "$B/game" --resolution 1600x1000 \
    --write-movie "$OUT/seed42-c3.avi" --fixed-fps 30 -- ~/.cache/the-unmapped/out/seed42-1024.db \
    --select c3 --play - - --speed 5 --quit-at-end --no-pane
```

Godot's own options (before `--`) choose the file (`.avi`: MJPEG with audio; `.png`: a numbered PNG sequence) and the frame rate; the viewer's options (after `--`) choose what plays. The movie file must be under the build root (V8); the viewer cannot see Godot's path, so this is a rule of the documented command, not a check. Recording needs a renderer (a desktop, or `xvfb-run` as above); no CI job records a movie (P4-00 6.2).

**Confirmation task** (viewer engineer, validation note): with the pinned 4.7.2 on Linux, record the command above to AVI and to PNG, check that the AVI plays in a common player and that its frame count is the `--play-log` frame count plus the 1 s lead-in and the 2 s end hold (90 frames at 30 fps) and the few frames before the world opens, and run it twice to show that the two `--play-log` outputs are identical. Windows: the same command in Git Bash without `xvfb-run`, by Paul or a later CI-free run (question 7). If Movie Maker proves unusable, the fallback of P4-00 6.2 applies (a PNG frame dump of the viewport by the viewer), with a spec amendment first.

## 10. Performance (P4-00 8; release, seed 42 at 1024; 2048 four times)

| Operation | Budget |
| --- | --- |
| `playback_advance` without its `set_year` (loop, ticker entries, flashes) | 1 ms |
| S_stop recomputation (play, filter change) | 2 ms at 3,000 events |
| a frame with a year change, `set_year` to the next drawn frame | 50 ms (P4-05 12.3), unchanged |
| playing on screen at 30 fps, any speed | under `xvfb-run`: no frame over 50 ms; on a desktop display: mean frame under 33 ms |

`--bench` adds, after its present lines, three simulated runs with dt = 33,333 (the model alone, headless and windowed): (i) [0, E], speed 200, skip off; (ii) [0, E], speed 10, skip on; (iii) the first civilization's story (`select_civilization` of the lowest id, `play(-1, -1)`), speed 5, skip on. Each prints

```
UNMAPPED-BENCH: playback <i|ii|iii>: <frames> frames, <changes> year changes, <stops> stops, movie <seconds> s at 30 fps, advance max <ms> ms (year <y>) mean <ms> ms, ticker max <ms> ms
```

and, windowed only, run (i) once more on real frames (vsync off, dt fixed at 33,333 regardless of delta) timing each frame from one `frame_post_draw` to the next:

```
UNMAPPED-BENCH: playback frames: mean <ms> ms, max <ms> ms, <n> over 33 ms, <k> over 50 ms
```

The validation note records the figures and the three movie lengths. A budget missed by more than a factor of two is a defect.

## 11. Headless check (P4-00 7 row P4-06, as amended in 13)

### 11.1 References and runs (`headless_check.cmake`)

No new renders. After the real world's check run, three short viewer runs on the real world, each with `--headless --fixed-fps 30`:

- **R1**: `-- <real> --play 0 200 --speed 100 --quit-at-end --play-log`: exit status 0; the `UNMAPPED-PLAY:` year lines ascend, the first is year 0, and the last line is `finished at 200 after <n> frames`.
- **R2**: R1 again: its `UNMAPPED-PLAY:` lines equal R1's byte for byte (a fixed frame step gives the same movie).
- **R3**: `-- <real> --play 0 <E + 1> --quit-at-end`: exit status 1 and the text `--play 0 <E + 1>: not a span of this world (0 to <E>)` in the output.

Each takes about a second after the import; the check stays one CTest entry under 60 s.

### 11.2 New checks (`viewer_check_playback.cpp`, in the same run as B0 to B12)

The check's **reference** is its own implementation of 4.2 over its own stop set: event years from the chronicle lines of `history-summary.txt` (summary) or `history.txt` (full), restricted as the case says; holder-change years from the check's own `Timeline` (the `load_history` of B8); phase starts from `[phases]` of `history.txt`. It never reads `PlaybackState`.

| Id | Check |
| --- | --- |
| P0 | without history (pattern and geography-only worlds): `%PlayButton`, `%PlaySpeed`, `%PlaySkipQuiet` disabled; `play`, `toggle_playback` false; state `stopped`; Space, Q, `-`, `=`, T do nothing; no ticker entries, `%Ticker` hidden |
| P1 | at open: state `stopped`, speed 10, skip on, ticker toggle on, `get_playback_speeds()` = `SPEEDS`, default span [0, E] with no selection |
| P2 | plain years: scope all, skip off, speed 10, `play(0, 120)`, then `playback_advance(100,000)` repeatedly: after call k (k ≤ 120) Y = k, one `year_changed` per call; call 121 emits `playback_finished` once and the state is `ended`; later calls change no year |
| P3 | timing against the reference, scope all, level summary, for C1 (speed 10, skip off, [0, 300], dt 33,333), C2 (speed 50, skip on, [0, E], 33,333), C3 (speed 500, skip on, [0, E], 16,667), C4 (speed 500, skip off, [0, E], 33,333): `get_playback_stop_years()` equals the reference's set; the sequence of (call, Y) of every year change equals the reference's; never more than one year change per call; every stop year appears; the call that ends playback is the reference's |
| P4 | story: the civilization of B5, level full, `select_civilization`, `play(-1, -1)`: span = [first, last year] of B3's list for it; with C2's speed, skip and dt, the stop years equal that list's distinct years plus P0 and P1; the ticker's event entries over the whole run are that list's events, in order, each once, and no other event. Again with group `war` set before `play`: the war events only |
| P5 | ticker, scope all, C2: at each arrival the new entries are, in order, the phase line (when a phase starts) and the rows of the summary chronicle's events of that year, text equal to `get_event_rows` of those ids; the same at level full against `history.txt`; at most 6 entries at any time; `current` true exactly for entries of Y; at the first year of the full chronicle with more than 6 events (if the world has one) the 5 + `... and <n> more` form, else the same rule checked on a ticker fed directly through `PlaybackState` |
| P6 | flashes, C2: the flashes added at each arrival are the ticker's events with a place, at B7's cell, colour `event_symbol_colour` of the kind; each is gone once its age reaches 1,200,000; never more than 32; none added for an event without a place |
| P7 | interaction: during C2, at the third stop year, `set_year(Y + 7)` from outside: state `paused`, Y = Y + 7, ticker = the entries of Y + 7 only, no flashes; `playback_advance` changes nothing while paused; `toggle_playback` resumes and the next year changes equal the reference restarted at Y + 7 with r = 0; `set_playback_speed(100)` while playing changes no year in that call and the dwell after it is 10,000; `select_event` of an event pauses at its year; `stop_playback` clears ticker and flashes; closing and reopening the world gives `stopped` |
| P8 | keys, with focus released: Space toggles `playing` / `paused`; Shift+Space stops; `-` and `=` step through `SPEEDS` and stop at both ends; Q toggles skip; T toggles the ticker's visibility; each change shows in `%PlayButton`, `%PlaySpeed`, `%PlaySkipQuiet` |
| P9 | options: `parse_play_options` over a table of argument lists gives the options or the exact texts of 9.2: valid spans, `-` at either end, from > to, to > E, a non-integer, one value only, every speed of `SPEEDS` and 0, 3 and 1000, `--skip-quiet maybe`, `--quit-at-end` and `--play-log` alone, with `--screenshot`, with `--bench`, and without history |
| P10 | layout: the screen laid out at 1280 × 800 and at 1600 × 1000 (as the check already lays it out), right pane shown: the slider widths and bounds of 8.1; `%Ticker` lies inside `%MapView` and is at most 40 % of its height |

P3 to P7 add about 20,000 calls of `playback_advance`, most without a year change, and about 5,000 year changes at 512 (about 2 s).

Not covered: whether the movie reads well, the ticker's legibility, the flashes' visibility, and the recorded file. Those are for the world critic's review of a recording (seed 42 at 1024: the whole history at speed 20 with skip on, and one ended civilization's story at speed 5) and screenshots of the ticker at both window sizes, and for Paul's walkthrough.

## 12. Invariants

1. A call of `playback_advance` makes at most one year change, and no stop year in the span is skipped.
2. The year sequence of a playback is a function of the world, the playback filter, the span, the speed, the skip flag and the sequence of dt values; no wall-clock time enters the model.
3. Playback never changes the browser's selection, filter, path or follow flag.
4. Every ticker text is a row text of `BrowserState` or a phase mark label with its year; the viewer composes no sentence (V3).
5. Every year-dependent view during playback is what `set_year(Y)` shows for the same Y (V6); nothing is drawn into a view image, so every pixel check of P4-03 and P4-04 is unchanged.

## 13. Amendments (applied by the lead at merge unless done here)

- **P4-00 7 row P4-06**: "skip quiet years visits only the years with events or holder changes plus the phase starts" reads: with skip on, quiet years take a tenth of the dwell, and with scope all the stop years are the event years of the list, the holder-change years and the phase starts (P4-06 3, 4); checks P0 to P10 and R1 to R3.
- **P4-03 4.1**: the year bar's layout is P4-06 8.1; 4.3: the keys of 8.2.
- **P4-05 6.5**: the keys of P4-06 8.2 are added; Space is not a browser key.
- **`gdext/README.md`** (viewer engineer): `PlaybackState`, the methods, keys, options, the recording command of 9.3 and its confirmation, and the bench lines.
- **CLAUDE.md, README** (lead): P4-06 merged; P4 complete pending Paul's review.
- **Proposals**: entry "Viewer v1, P4-06: how the history movie keeps time" (added with this spec).

## 14. Open questions for Paul

Each has a recommendation, which the implementation follows until he decides.

1. **Skip quiet years: faster or jumped.** Recommendation: quiet years at ten times the speed (Q = 10), so the slider visibly runs through a long peace and the settlement markers still grow. Alternative: jump straight from stop to stop (Q infinite), which gives the shortest movie.
2. **The movie plays the browser's list.** Recommendation: playback walks the list the browser shows (3, 5.4), so a civilization's story is "select, Play", and a group filter plays only its wars. Alternative: playback always plays the whole history, and the selection only colours the ticker.
3. **Scrubbing during play pauses.** Recommendation: any year change not made by playback pauses at the new year, and Play resumes from there (4.4). Alternatives: scrubbing stops playback, or playback continues from the new year.
4. **The ticker over the map.** Recommendation: a translucent panel over the map's lower edge (6.2), so a recording with the right pane hidden shows map and sentences together. Alternatives: a strip under the map (the map loses about 130 px of height at 1600 × 1000), or the ticker in the right pane.
5. **Defaults.** Recommendation: speed 10 years a second, skip on, ticker on; speeds 1 to 500 in nine steps. A whole history then plays in a minute or two (the bench records the real length); a story reads well at 2 to 5.
6. **The year bar's two rows.** Recommendation: the slider on a row of its own with Play, speed and skip (8.1), which fixes the 10 px slider at 1280 × 800 for one row of map height. Alternative: shorter button labels on one row, which leaves the slider narrow at 1280.
7. **Recording format and Windows.** Recommendation: AVI (MJPEG) at 30 fps for review movies, PNG sequence when an editor will cut them; the engineer confirms Linux, and Paul confirms the documented command on Windows once, since no CI job records. Alternative: a CI job that records a short PNG sequence on both systems (minutes of runner time per push).

## 15. Readings chosen

1. "Speed in years per second" is the rate of years at which the ticker has something to say; years with nothing listed are the ones that skip quiet years speeds up (3).
2. "The sentences of each year in a ticker beside the map" (P4-00 3.1, design 10): the ticker is over the map's lower edge rather than beside it, so that it is in a recording of the map (question 4).
3. "Event places flashing (P4-04)": a flash is a short expanding ring at the P4-04 place of each listed event of the year, in its ring colour, on top of P4-04's rings (7).
4. "Quit at the end": a hold of two seconds on the last year, so the movie ends on a readable frame (9.1).
5. A deterministic recording means the same years in the same frames for the same command, not the same pixels across platforms (V4); P3 and R2 test the former.

## Revision notes

- **r1** (2026-11-23): first draft.
