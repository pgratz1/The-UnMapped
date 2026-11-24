# P4-05 Browser of civilizations and events

Status: **r2, 2026-11-23**: correction round, the text brought in line with what was built and ruled (revision notes at the end). r1 was approved by the lead and built on `p4/05-*` with P5-02 merged in between, so the stage merged at generator 0.23.0, schema 9. Phase P4 Viewer v1, stage 5 of the overview `docs/specs/p4-00-viewer-v1-overview.md` (3.1 "P4-05", 3.2, rules V1 to V10, 7 row P4-05, 8, 9, 10). Builds on P4-01 (`Timeline`, `LoadedHistory`), P4-03 r4 (year bar, `HistoryState`, H1 to H9), P4-04 r2 (`MarkerIndex`, markers, hover, M0 to M9) and P3-07 (the digest, `[civilizations]`, the chronicle line), as on `origin/main` at 61d2e23 (generator 0.22.0, schema 8). Design doc 10 ("World viewer": "a browser for civilizations, figures and events with their causes") and 13 (P4 row and exit check). Section 12 carries the world critic's P4-04 findings and the slow year change.

**No world byte changes.** Generator stays **0.22.0**, schema stays **8**; `report` and `hash` bytes unchanged. New golden file `worldgen/tests/golden/p4-05-browser.txt`. The carried-over glyph and colour changes (12.2) re-record `p4-04-markers.txt` and change the `markers.txt` hashes of `world-hashes.sh`; nothing else moves.

## 1. Purpose

After this stage the viewer has a browser beside the map. It shows every civilization (name, race, years, fate) and, for the one selected, its block as `report --history` prints it. It lists events (the summary set or all of them, filtered by civilization, place, kind group and years), each as its chronicle sentence and note, and shows one event's participants, values, causes and effects as links. Selections are tied to the map and the year bar. Selecting a civilization dims the land it does not hold at Y, frames its settlements and turns on **follow**, so that the event steps walk its events. Selecting an event sets Y to the event's year and marks its place. Clicking a marker or a region on the map selects that settlement or region in the browser. This is the stage that meets the phase's exit check (P4-00 10): Paul follows one civilization from awakening to the present without leaving the viewer.

**Rule (P4-00 V3, 6.1): the library answers, the viewer draws.** Everything the browser shows as text, every list and filter result, every link target and the place of an event come from `worldgen`. The viewer parses no report text, issues no SQL, tests no kind by id and restates no report rule. The report's `[civilizations]` blocks are printed from the same structured lines the viewer shows, so the two cannot drift apart (5.2).

## 2. Parts

| Part | Area | Files | Owner |
| --- | --- | --- | --- |
| **A library** | linked sentences | `worldgen/include/worldgen/history/event_catalogue.hpp`, `src/history/event_catalogue.cpp` (`render_event_linked`) | generator engineer |
| A | block lines, digest entry | `worldgen/include/worldgen/inspect/history_report.hpp`, `src/inspect/history_report.cpp` (refactor, bytes unchanged) | generator engineer |
| A | browser index | new `worldgen/include/worldgen/inspect/history_browser.hpp`, `src/inspect/history_browser.cpp` | generator engineer |
| A | event place, glyph and colour items | `worldgen/inspect/markers.hpp`, `src/inspect/markers.cpp` (4.6, 12.2) | generator engineer |
| A | tests | `worldgen/tests/browser_*.cpp` (SHARD `browser`, pattern `*browser_*`; SLOW `slow-browser`), fixture `worldgen/tests/data/browser/`, reference `worldgen/tests/reference/p4_browser_reference.py`, golden `p4-05-browser.txt`; `p4_markers_reference.py` and `p4-04-markers.txt` for 12.2 | test writer |
| **B viewer** | model, screen, scene, check | `gdext/src/viewer/*` (new `browser_state.*`), `game/viewer/viewer.tscn`, `gdext/cmake/headless_check.cmake`, `gdext/README.md` | viewer engineer |

Part A goes through the generator engineer, test writer, determinism auditor and validation specialist, and merges first. Part B starts on its own branch at once against the headers of section 4. Only its checks that compare with the library's answers (B2 to B4) wait for Part A's merge. The first task of Part B is the year-change investigation of 12.3, which needs nothing from Part A. The world critic reviews Part B's screenshots and the walkthrough (P4-00 3.2).

## 3. What is shown, in words

- **Civilization list**: one row per `civilization` row, ascending id (the report's order): colour swatch, name, race name, `<founded>-<ended>` or `<founded>-`, fate word `alive` or `ended`.
- **Civilization block**: the lines of its `[civilizations]` block at the browser's level (summary or full, 6.4), without the report's escapes and tags, with links on every reference.
- **Event list**: one row per event of the current filter, ascending id. Row text: `<span>  <sentence>[ (<note>)]`, where `<span>` is `<year>` or `<year>-<end_year>` and the sentence and note are those of the chronicle line (`history_note(view, e, 0)`).
- **Event detail**: the row text with the sentence's names linked; `kind <key>, salience <s>[, in the summary]`; `participants:` one line per participant, `<role>: <name>` linked; `values:` `<key> = <value>` lines; `caused by:` and `led to:`, one line per event, each a link with its row text. The heading also links the event's settlement and region.
- **Place detail** (a settlement or region selected): the P4-04 hover text of that settlement at Y, or `<region name at Y>   |   <holding text>` (P4-03 6.1), followed by the event list filtered to it.

Monsters, sites and figures are not built. Section 9 says where they attach.

## 4. Library API (Part A)

Integers only, no Godot types, data errors as `db::Result` values, FatalError only for a caller's bad id or level. No new SQL: everything comes from `LoadedHistory` (P4-01) and `MarkerIndex` (P4-04).

### 4.1 Linked sentences (`worldgen/history/event_catalogue.hpp`)

```cpp
struct TextLink {
    std::uint32_t start = 0, length = 0;   // byte offsets into the text (UTF-8)
    std::int64_t type = 0;                 // 0 event; 1..9 entity type code (EntityType)
    std::int64_t id = 0;
    bool tagged = false;                   // block references only (4.2); always false in a sentence
    friend bool operator==(const TextLink&, const TextLink&) = default;
};
struct LinkedText {
    std::string text;
    std::vector<TextLink> links;           // ascending start, never overlapping
    friend bool operator==(const LinkedText&, const LinkedText&) = default;
};
[[nodiscard]] LinkedText render_event_linked(const HistoryView& view, std::int64_t event_id);
```

`render_event` becomes `render_event_linked(...).text`: one code path, so its bytes cannot change. A link is added for every name the renderer appends. For a role token in form `name` or `all`, the link covers each participant's name as written, including a lower-cased `the `, with the participant's type and id. For forms `plural` and `adjective`, it covers the race word, with type 1 and the race id. For `{region}` and `{region_former}`, it covers the region name as written, with type 5. For `{settlement}`, type 4. Values, years, choices and cause words get no link. FatalErrors are those of `render_event`.

### 4.2 Block lines (`worldgen/inspect/history_report.hpp`)

```cpp
enum class SegmentKind : std::uint8_t { literal = 1, text = 2, entity = 3, event = 4, sentence = 5 };
struct Segment {
    SegmentKind kind = SegmentKind::literal;
    std::string text;       // literal / text: the bytes; entity: the name (empty when it has none)
    std::int64_t type = 0;  // entity: type code
    std::int64_t id = 0;    // entity: id; event, sentence: event id
};
using BlockLine = std::vector<Segment>;

// The lines of civilization `civ`'s block in [civilizations] at `level`, in
// print order, the first being "c<id> <name>". FatalError for an unknown civ.
[[nodiscard]] std::vector<BlockLine> civilization_block(const history::HistoryView& view,
    const history::HistoryTables& tables, const history::HistoryDigest& digest,
    std::int64_t civ, HistoryLevel level);

// Report form: literal as is; text escape_history_text(text); entity
// escape_history_text(name) + " (" + tag + ")", or the tag alone when the name
// is empty; event "e<id>"; sentence escape_history_text(render_event(view, id)).
[[nodiscard]] std::string block_line_report_text(const history::HistoryView& view, const BlockLine& line);
// Display form: the same without escapes and without " (<tag>)"; entity links
// tagged true, event links over "e<id>", a sentence's own links (4.1) shifted.
[[nodiscard]] history::LinkedText block_line_display(const history::HistoryView& view, const BlockLine& line);

// The digest the report prints with (today digest_history; see 4.3).
[[nodiscard]] history::HistoryDigest history_report_digest(const history::HistoryView& view,
                                                           const history::HistoryTables& tables);
```

The tag is the report's `ref` prefix plus the id (`ra`, `g`, `c`, `s`, `rg`, `f`, `fa`, `m`, `si`), given by `history_entity_tag(type, id)` (public, `history_report.hpp`; `?<id>` for a type code outside 1 to 9). A region's name is its name at E, as `ref` gives it today.

As built (r2): an entity segment with an empty name shows its tag in the display form, as an **untagged** link over the tag (the report form prints the tag alone), so B4's transform still gives the report form. `civilization_block` throws FatalError `civilization_block: no civilization <c>` for an unknown civilization and `civilization_block: bad level` for a level that is neither summary nor full. A note is a `text` segment. Each `" (" + note + ")"` in a milestone line is split into a literal `" ("`, a text and a literal `")"`.

**The report is refactored to print its blocks from these lines.** `Report::civilization` becomes a loop: `line(block_line_report_text(view, l))` over `civilization_block(...)`. Its bytes must stay the same. The P3-07 goldens (`p3-07-report.txt`), the `history.*.txt` files of `world-hashes.sh` and the P3-07 test suites prove it, and any difference is a defect. `build_history_report` calls `history_report_digest`.

### 4.3 The digest entry, and P5-02

Today `history_report_digest` = `digest_history`. On `origin/p5/02-impl-r1` (generator 0.23.0, schema 9), the report chooses the summary set a second time, with a budget that depends on its own line counts (P5-02 12.9, `choose_summary_set`). With that change, `history_report_digest` becomes the function that returns the digest after that second choice, and the report and the browser both call it. Whichever of P4-05 Part A and P5-02 merges second moves the step into `history_report_digest`. The browser must never call `digest_history` directly. `render_event_linked` must likewise cover any token P5-02 adds (form `kind`: a link to the monster, type 8). Whichever merges second adds it. (r2: both done. P5-02 merged first; `history_report_digest` makes the second choice, and a `kind` token is a link to the monster, type 8.)

### 4.4 Kind groups (`history_browser.hpp`)

```cpp
enum class EventGroup : std::uint8_t { realms = 0, settlements = 1, war = 2, calamity = 3, other = 4, monsters = 5 };
inline constexpr std::uint32_t ALL_GROUPS = 0x3F;
[[nodiscard]] EventGroup event_group(std::string_view kind_key) noexcept;      // bytes; default other
[[nodiscard]] std::string_view event_group_word(EventGroup) noexcept;          // "realms" ... "other", "monsters"
```

| Group | Kind keys |
| --- | --- |
| `realms` | `awakening`, `civ_founded`, `civ_seceded`, `civ_ended`, `civ_conquered`, `capital_moved`, `succession_crisis`, `quiet_era`, `quiet_stretch`, `region_renamed` |
| `settlements` | `settlement_founded`, `colony_founded`, `became_village`, `became_town`, `became_city`, `settlement_dwindled`, `settlement_abandoned`, `settlement_abandoned_to`, `ruin_faded`, `refugees_settled`, `refuge_hidden` |
| `war` | `flare_up`, `claim`, `claim_settlements`, `claim_dropped`, `war_declared`, `battle`, `battle_at_pass`, `siege`, `sacked`, `settlement_captured`, `peace`, `truce`, `war_won`, `region_ceded`, `settlements_ceded`, `tribute`, `search_abandoned`, `refuge_held` |
| `calamity` | `famine`, `plague`, `plague_spread`, `plague_outbreak` |
| `other` | any other key (V7: kinds by key) |
| `monsters` | `monster_appeared`, `undead_rose`, `monster_woke`, `monster_slept`, `monster_raid`, `settlement_overrun`, `monster_slain`, `monster_driven_off`, `monster_died`, `beasts_bold`, `outpost_built`, `outpost_abandoned` (P5-02's kinds 44 to 55) |

This covers every one of the 55 shipped kinds (lead ruling at the P4-05 integration with P5-02: group `monsters` appended as code 5, `ALL_GROUPS` = 0x3F; codes are never reused). No shipped kind falls to `other`. A new kind gets its row when it is added. The test in 10.2 item 2 fails if a compiled kind has no row.

### 4.5 The browser index (`worldgen/inspect/history_browser.hpp`)

```cpp
struct CivilizationEntry {
    std::int64_t id = 0, race = 0, founded_year = 0;
    std::int64_t ended_year = -1, ending_event = 0;   // -1 / 0 while alive
    std::int64_t events = 0;                           // CivilizationDigest::events
    std::string name, race_name;
    friend bool operator==(const CivilizationEntry&, const CivilizationEntry&) = default;
};
struct EventFilter {
    std::int64_t civilization = 0, settlement = 0, region = 0;  // 0: any; several: all must hold
    std::int64_t from_year = -1, to_year = -1;  // -1: open; kept when year <= to and end_year >= from
    std::uint32_t groups = ALL_GROUPS;          // bit (1 << group)
    bool summary_only = true;                   // the summary set of the report's digest
};
struct EventEntry {
    std::int64_t id = 0, year = 0, end_year = 0, kind = 0, salience = 0, region = 0, settlement = 0;
    EventGroup group = EventGroup::other;
    bool summary = false;
};
struct Participant { std::string role; std::int64_t type = 0, id = 0; std::string name; };
struct EventDetail {
    EventEntry entry;
    std::string kind_key;
    history::LinkedText sentence;           // render_event_linked
    std::string note;                       // history_note(view, id, 0), unescaped
    std::vector<Participant> participants;  // seq order; name: region at the event (as the sentence), else HistoryView::name, else empty
    std::vector<std::pair<std::string, std::int64_t>> values;  // seq order
    std::vector<std::int64_t> causes;       // ascending (HistoryView)
    std::vector<std::int64_t> effects;      // ascending (Timeline::effects)
};
struct NameMatch { std::int64_t type = 0, id = 0; std::string name; };

class BrowserIndex {
public:
    // Digest (history_report_digest), the entries, and per-event group and summary flag.
    // std::bad_alloc: out_of_memory "out of memory while building the browser". Holds no
    // reference into `history`.
    [[nodiscard]] static db::Result<BrowserIndex> build(const history::LoadedHistory& history);

    [[nodiscard]] std::span<const CivilizationEntry> civilizations() const noexcept;  // ascending id
    [[nodiscard]] std::span<const EventEntry> events() const noexcept;               // ascending id
    [[nodiscard]] const history::HistoryDigest& digest() const noexcept;

    void select(const history::LoadedHistory& history, const EventFilter& filter,
                std::vector<std::int64_t>& out) const;            // ascending id; reuses out
    [[nodiscard]] EventDetail event_detail(const history::LoadedHistory& history, std::int64_t event) const;
    [[nodiscard]] std::vector<history::LinkedText> block(const history::LoadedHistory& history,
                                                         std::int64_t civ, HistoryLevel level) const;
    // Row text of section 3, unescaped: "<span>  <sentence>[ (<note>)]".
    [[nodiscard]] std::string row_text(const history::LoadedHistory& history, std::int64_t event) const;
    [[nodiscard]] std::vector<NameMatch> search(const history::LoadedHistory& history,
                                                std::string_view query, std::size_t limit) const;
};
```

Rules:

- **select**: the candidates are `Timeline::events_of_civilization`, `events_of_settlement` or `events_of_region` for the first non-zero entity field, else every event. Each candidate is kept when it is in every other non-zero entity list (a merge of sorted lists), in the year span, in `groups`, and, when `summary_only`, has `digest.summary[id − 1]` = 1. An entity id the world lacks gives FatalError `browser: no <type> <id>`.
- **block**: `block_line_display` of each line of `civilization_block(view, tables, digest, civ, level)`.
- **search**: the query is bytes. Comparison is ASCII case-insensitive (A–Z folded to a–z; every other byte compared as is), and a match is a substring of a name. Names are searched in this order: civilizations by id, settlements by id, then regions by id. A region tries its name at E, then its geographic name, then its `region_name` rows by ascending `from_year`, and the first name that matches is reported. There is at most one match per (type, id), and at most `limit` matches in that order. An empty query gives none.
- **Errors** (as built): FatalError `browser: no event <e>`, `browser: no civilization <c>`, `browser: no settlement <s>`, `browser: no region <r>` (from `select`'s entity fields, `event_detail`, `row_text` and `block`), `browser: bad level` for a caller's bad argument; `browser: the history is not the one the index was built from` when a call passes another history than `build` had. Nothing else throws on a history `load_history` accepted.

### 4.6 The place of an event (`worldgen/inspect/markers.hpp`)

```cpp
// P4-04 3.4: the settlement's cell, else the region's seed cell, else none.
[[nodiscard]] std::optional<std::array<std::int64_t, 2>> MarkerIndex::event_place(
    const history::LoadedHistory& history, std::int64_t event) const;   // FatalError "markers: no event <e>"
```

`markers_at` uses it for its places, so the rule is written once. `p4-04-markers.txt` lines that 12.2 does not move stay as they are.

### 4.7 Cost (release; budgets for a 3,000-event world)

The budgets apply to a world of about 3,000 events only (r2).

| Call | Budget at 3,000 events |
| --- | --- |
| `BrowserIndex::build` | 10 ms (one pass; the digest is the report's, about 5 ms) |
| `select` | 1 ms |
| `event_detail`, `row_text` | 0.1 ms |
| `block` (full) | 1 ms |
| `search` | 2 ms |

These are measured in the skipped suite `browser-bench` (`worldgen/tests/p4_05_engineer_test.cpp`, run by hand with `worldgen-tests -ts=browser-bench --no-skip`; prints only). The report's time is unchanged within noise.

**Measured** (r2, the engineer's runs on `linux-gcc-release`, generator 0.23.0): on the default 1024 world `sample-42-1024`, `BrowserIndex::build` takes 4 to 9 ms. The bench's large world (seed 42 at 1024, detail dense, intensity turbulent, four civilizations per race, span 12,000 years) has 8,685 events, almost three times the budget world: there `build` takes 26 to 39 ms and the full block of its largest civilization 3 to 6 ms. No budget applies to a world of that size or to the default world: the table's budgets are for the 3,000-event world only.

## 5. Viewer model (Part B)

### 5.1 State (`BrowserState`, C++ only, owned by the model beside `HistoryState`)

It is built on open after `HistoryState::load` and `MarkerIndex::load` succeed, with `BrowserIndex::build` and the row texts of every event (one `row_text` each, about 3 ms at 3,000). A failure leaves history and markers working, with the browser unavailable and the file label gaining `browser: <code>: <message>` (V10). It holds, across years:

- the **selection** `{entity_type, entity_id, event, place_type, place_id}`. The entity is a civilization (3) now, and 8 and 9 later (section 9). The place is a settlement (4) or region (5). 0 means none;
- the **filter** `{scope: all | entity | place, group: all | one EventGroup, level: summary | full, from, to, this_year}`;
- the current id list (`select`) and its row texts;
- **follow** (on or off);
- the **path**: the back and forward stacks of selections plus the year at each step, at most 100 entries (the oldest is dropped);
- the highlight sets of 5.4.

Nothing year-dependent is cached across a change of Y except as 5.4 says (V6).

### 5.2 Bound methods and signals (`ViewerModel`)

Every method is valid without history or browser. It then answers empty, −1 or false.

| Method | Returns / does |
| --- | --- |
| `get_browser_available()` | history, markers and browser loaded |
| `get_civilization_list()` | Array of `{id, name, race, race_name, colour, founded_year, ended_year, fate, events}`; `fate` `alive`/`ended`; `colour` as `get_civilization_info` |
| `get_civilization_block(civ)` | Array of `{text, links}` at the current level; `links` Array of `{start, length, target, tagged}`; `start`, `length` in **characters** (the viewer converts byte offsets, so RichTextLabel can use them); `target` is the tag (`c3`, `s12`, `rg4`, `ra2`, `e57`, …) |
| `get_event_ids()`, `get_event_rows()` | PackedInt64Array and PackedStringArray of the current list, same order |
| `get_event_detail(e)` | Dictionary `{id, row, kind_key, group, salience, summary, sentence {text, links}, note, participants [{role, target, name}], values [{key, value}], causes [{target, row}], effects [{target, row}], place: Vector2i or (−1, −1), settlement, region}` |
| `set_browser_scope(s)`, `set_browser_group(g)`, `set_browser_level(l)`, `set_browser_years(from, to)`, `set_browser_this_year(on)` | change the filter. `g`: `all` or a group word; `l`: `summary`/`full`; false for a bad value |
| `select_civilization(c)` | entity = c (0 clears), scope `entity`, follow on (off when cleared), place cleared; pushes the path |
| `select_event(e)` | event = e; `set_year(e.year)`; pushes the path |
| `select_place(type, id)` | type 4 or 5; scope `place`; pushes the path |
| `follow_link(target)` | `c<id>`: `select_civilization`; `s<id>`, `rg<id>`: `select_place`; `e<id>`: `select_event`; any other tag: false (`ra`, `g` and, until section 9, `m`, `si`, `f`, `fa`). As built: `can_follow_link(target)` answers whether a target has one of the four followable forms, and the screen renders only those as links (underlined, `push_meta`); every other reference is plain text |
| `browser_back()`, `browser_forward()`, `can_browser_back()`, `can_browser_forward()` | restore the previous / next path entry: selection, filter scope and year |
| `set_follow(on)`, `get_follow()` | follow (5.3) |
| `follow_first()` | `select_event` of the first id of the current list; false when it is empty |
| `search_names(q)` | Array of `{target, name, kind}` from `search(q, 50)`; `kind` `civilization`, `settlement`, `region` |
| `get_selection()` | Dictionary of 5.1's selection plus `scope`, `group`, `level` |
| `get_highlight()` | Dictionary `{civilization, regions, settlements, place, place_event}` (5.4) |

Signals: `browser_selection_changed()`, `event_list_changed()` (the id list differs from the previous one), `highlight_changed()`.

### 5.3 Follow and the year bar

- **Follow** walks the **current event list**. With a civilization selected and scope `entity`, that is its events. With a place selected, it is the place's events. While follow is on and the list is not empty, `step_event(±1)` (the year bar's `< event` / `event >`, Page Up / Page Down) selects the previous or next id of the list and sets Y to its year. From a selected event that is in the list, the step goes to its neighbour in the list. Without one, `+1` goes to the first id with year > Y and `−1` to the last id with year < Y. `has_previous_event()` and `has_next_event()` answer the same way. Several events of one year are visited one by one, so Y may stay the same; `year_changed` is emitted only when Y changes.
- With follow off, or the list empty, the steps are P4-03's (any event year).
- Holder-change steps, ±1, ±25, the slider and the field never change the selection. A selected event whose span no longer contains Y stays selected but loses its place mark (5.4).
- **Walkthrough** (P4-00 10 steps 2 to 5): choose a civilization in the list, which turns follow on. **First event** selects its first event (its founding or the awakening) and jumps to that year. `event >` walks to the present. A cause link and **Back** return along the path.

### 5.4 Highlight (presentation; data from the library)

- **Civilization c at Y**: `regions` are the regions whose `YearState::regions` holder is c, and `settlements` are the standing marker entries whose owner is c. They are recomputed on a year change and emit `highlight_changed` only when either list differs (dragging over years where c's land is unchanged redraws nothing).
- **Event e**: `place` is `event_place(e)` while Y is in [e.year, e.end_year], otherwise (−1, −1).
- **Place**: a settlement's cell, or nothing for a region (its land is drawn as a held set of one region).

### 5.5 Map clicks

`LayerMapView` gains `cell_clicked(x, y, marker_index)`: a left press and release with less than 4 screen pixels of movement between them (a drag still pans; a double click still fits). The screen maps it as follows. On a `standing`, `ruin` or `faded` marker: `select_place(4, id)`. On a `place` ring: `select_event` of its smallest event. On a pass: nothing. Off any marker, on a cell whose `region` value is a region of the timeline: `select_place(5, region)`. Otherwise nothing.

## 6. Screen (Part B; `viewer.tscn` holds layout only)

### 6.1 Layout

`Split` gets a third pane: `Sidebar (340) | MapArea | RightPane`. `RightPane` is a `TabContainer` (min width 380, default 440). Its tabs are **Browser** and **Legend**, and Legend is the existing `LegendPanel` moved unchanged (unique names kept). A world with history opens on Browser, and a world without history opens on Legend with the Browser tab disabled. At **1600 × 1000** the map gets about 800 × 860 pixels. A 1024 map then fits at z ≈ 0.79, against 0.84 before (the map was height-bound), about 6 % smaller. At **1280 × 800** the right pane takes its 380 minimum and the map about 540 × 660. Key **B** hides or shows `RightPane`, giving the map the space. The split positions are not saved (V2: no settings yet).

```
RightPane (TabContainer)
  Browser (VBoxContainer)
    BrowserTop (HBox): %BrowserBack "<"  %BrowserForward ">"  %BrowserSearch LineEdit (expand, placeholder "Search names")
    %EntityTabs TabBar ("Civilizations"; section 9)
    %CivList Tree (4 columns: name with swatch, race, years, fate; stretch 0.30)
    %SearchResults ItemList (hidden while the search text is empty; replaces %CivList while shown)
    EventFilters (HBox): %EventScope OptionButton (All / Selected / Place)  %EventGroup OptionButton (All kinds, then the words of get_event_groups() in code order: realms / settlements / war / calamity / other / monsters)  %EventAll CheckBox "All events"
    EventYears (HBox): %EventFrom SpinBox  %EventTo SpinBox  %EventThisYear CheckBox "This year"  %FollowToggle CheckBox "Follow" (r2: a check box, as built, so the row fits 1280 × 800)  %FollowFirst Button "First event"
    %EventList ItemList (one column, text trimmed, tooltip the full row; stretch 0.40)
    %BrowserDetail RichTextLabel (selectable text, stretch 0.30)
  Legend (the moved LegendPanel)
```

`%BrowserDetail` is built with `push_meta(target)`, `add_text` and `pop`, never by parsing BBCode, so world text cannot inject markup. A `meta_clicked` calls `follow_link`. The civilization block is shown in `%BrowserDetail` when a civilization is selected and no event is. Otherwise the event's detail is shown, with the selected civilization's name as a link at the top.

### 6.2 Map drawing (`LayerMapView`, after the image, in this order)

1. **Dimming**: with a civilization selected, the cells outside `regions` are drawn with black at alpha 0.45. The mask texture (map size, one byte per cell, from the decoded `region` layer) is rebuilt only on `highlight_changed`.
2. Markers (P4-04).
3. **Frames**: a 2 px square in (255, 215, 0) around the glyph of each entry in `settlements`, and a 2 px square in (0, 230, 255) around the selected place settlement.
4. **Event place**: two concentric 2 px squares (white inside, black outside) of half-size 12 g screen pixels around `place`.

No highlight is drawn into a view image, so every pixel comparison of P4-03 and P4-04 is unchanged.

### 6.3 Behaviour

- `event_list_changed` rebuilds `%EventList` from `get_event_rows()`. A **year change never rebuilds it**, except with **This year** on (then the filter span is [Y, Y] and the list is rebuilt only when the ids differ). On a year change the list scrolls to the first row with year ≥ Y (a binary search over the ids) without changing the selection.
- Selecting a row calls `select_event`. Selecting a civilization row calls `select_civilization`. Selecting a search result calls `follow_link`.
- `%FollowToggle`, `%FollowFirst`, Back and Forward reflect `get_follow()`, an empty list and `can_browser_*()`.
- Typing in `%BrowserSearch` calls `search_names` on every change (at most 50 rows).

### 6.4 Level

`%EventAll` sets level `full` (all events, and the block at `full`, with its `relations:` lines). Off is `summary` (the summary set, and the block at summary with `adversaries:`, short for an ended realm). So what the browser shows always equals one of the two reports. The default is summary.

### 6.5 Keys (`_unhandled_key_input`; P4-03 4.3 focus rule)

| Key | Action |
| --- | --- |
| F | `set_follow(!get_follow())` |
| Backspace / Shift+Backspace | `browser_back()` / `browser_forward()` |
| Escape | if a browser control has focus, release it; else clear the selection (pushes the path) |
| Ctrl+F | focus `%BrowserSearch` |
| B | hide or show `RightPane` |
| Page Up / Page Down | `step_event(∓1)`, follow-aware (5.3) |

The lists, the search field and the spin boxes take focus when clicked. While one has focus, the arrow keys act on it (P4-03 4.3), and Escape releases it.

## 7. Performance (P4-00 8; release, seed 42 at 1024 and a 3,000-event world)

| Operation | Budget |
| --- | --- |
| Open: extra time over P4-04 (index, row texts) | 50 ms (3,000 events) |
| Filter change to the redrawn list | 20 ms at 3,000 rows (P4-00 8); 50 ms at 2048 |
| Selection change to the next frame (detail, highlight mask, year) | 30 ms, plus the year change it causes |
| Year step with a civilization selected | browser part 1 ms (highlight lists, no list rebuild); mask rebuild 10 ms, only when the held set changes |
| Year change to the next frame | 50 ms (12.3) |

`--bench` adds the build time of the index and row texts, the slowest and mean `select` with `get_event_rows` and `%EventList` rebuild at full / all (3,000 rows on the large world), the number of `event_list_changed` and `highlight_changed` emissions over 0 … E and back with the first civilization selected, and the slowest mask rebuild. The validation note records them. A budget missed by more than a factor of two is a defect.

The large world for budgets is `generate --seed 42 --map-size 1024 --event-intensity turbulent --civs-per-race 3` (r2: the CLI has no intensity `high`; `turbulent` is its highest). It has 2,646 events at generator 0.23.0; `gdext/README.md` records it with the measured figures.

## 8. Headless check (P4-00 7 row P4-05)

### 8.1 References (`headless_check.cmake`, real world)

In addition to P4-03 and P4-04: `report --history --level summary --out <real>/history-summary.txt`. The viewer run adds `--history-summary <path>`. No new renders.

### 8.2 New checks (`viewer_check.cpp`)

| Id | Check |
| --- | --- |
| B0 | without history: `get_browser_available()` false; every browser control disabled; the Browser tab disabled and Legend selected; every method of 5.2 empty or false |
| B1 | civilization list = the `[civilizations]` blocks of `history.txt`: per block, `c<id> <name>`, race id from `  race: <Name> (ra<id>)`, founded from `  founded:`, fate and ended year from `  fate:`; same order; `events` = `  events: <n>`; each swatch = `colour` |
| B2 | every event id: `escape(sentence.text)` + (`" (" + escape(note) + ")"` when the note is not empty) equals the text between `<key>/<salience>: ` and ` {` of its chronicle line at full; kind key, salience and span equal the line's; `causes` equals `[causes …]`; participants and values give exactly the line's `{…}` refs (`role=<tag>`, `key=<v>`, `@rg`, `@s` from `region`, `settlement`); `effects` is the inverse of every line's causes; `summary` is true exactly for the ids in `history-summary.txt`'s chronicle |
| B3 | lists: scope all, groups all: level full gives every chronicle id, and level summary the summary chronicle's ids; for each civilization at full: the ids of chronicle lines with a participant `=c<id>`, whose count equals B1's `events` |
| B4 | blocks: for each civilization at both levels, the display lines turned back into report form equal its block in `history.txt` / `history-summary.txt` line by line. The check escapes every byte outside tagged links and replaces each tagged link's text t with `escape(t) + " (" + target + ")"`. This checks the links' offsets, targets and tags as well as the text |
| B5 | follow (the walkthrough): first ended civilization by id; level full, group all; `select_civilization`, then `follow_first()`, then `step_event(+1)` until it returns false: the selected ids, in order, are B3's list for it; Y after each step is that event's year; `year_changed` counted only on year changes; `step_event(−1)` from the last returns to the one before. With follow off, `step_event(+1)` from the same year is P4-03's step |
| B6 | links: the first event in id order with a cause that itself has a cause; `follow_link("e<cause>")` twice selects each cause in turn and sets Y to its year; `browser_back()` twice returns to the start event and its year; `browser_forward()` once goes to the first cause; `follow_link` of a `c` link selects that civilization |
| B7 | selecting the first, a middle and the last event sets Y to its year; `get_highlight().place` equals the cell of M4's rule (`@s` cell from `[present]`, else the `@rg` seed cell); a year outside its span clears `place` |
| B8 | map: a simulated click (press, release) at the screen position of the first `standing` and the first `ruin` marker selects place (4, id), with list = `Timeline::events_of_settlement` (the check's own `load_history`) at the current filter; a click on a ring selects its smallest event; a click on a region cell away from every marker selects (5, region); a press, a 20 px drag and a release selects nothing and pans |
| B9 | highlight: for the civilization of B5 at each Y in LM (P4-04 9.1): `regions` = the regions whose `get_region_holding` names it; `settlements` = the `standing` entries of `get_markers()` with its owner; stepping `set_year` over 0 … E emits `highlight_changed` exactly in the years where either list differs from the year before |
| B10 | no rebuilds: stepping `set_year` over 0 … E with the civilization selected emits no `event_list_changed`; with This year on it emits exactly in the years whose event ids differ from the year before |
| B11 | search: the first three bytes of each civilization's name, lower-cased, give a result list containing that civilization; selecting it selects it; an empty query gives no rows |
| B12 | keys, with focus released: F toggles follow; Backspace is `browser_back()`; Escape clears the selection; B hides and shows `RightPane` |

P4-03 and P4-04 checks run unchanged, except where 12.2 changes a key-list label (M1) or a hover text (M7). The check stays one CTest entry under 60 s on `linux-gcc-release`. B2 and B4 are text comparisons over about 650 events and 10 blocks, and B9 and B10 add about 12,000 year steps (about 2 s at 512).

Not covered: legibility of the panel, the dimming and the frames at both window sizes, and whether following reads well. Those are for Paul's walkthrough and the world critic's screenshots at 1600 × 1000 and 1280 × 800.

## 9. Monsters, sites and figures (where they attach; not built)

P5-02 (generator 0.23.0, schema 9) adds monsters (type 8) and sites (type 9), the timeline reads `sites_existing`, `site_at` and `monster_at`, and report blocks `[monsters]` and `[sites]`. When the library provides them:

- **Lists**: `%EntityTabs` gains `Monsters` and `Sites` tabs. Each is a list like `%CivList`, from new `BrowserIndex` entries built from the report's block lines (`monster_block`, `site_block` in the form of 4.2, so that the report prints from them too).
- **Selection**: already generic (`entity_type`, `entity_id`). `select_entity(type, id)` replaces `select_civilization`, which stays as a wrapper. `follow_link` accepts `m` and `si`.
- **Filter**: `EventFilter` gains `monster` and `site` (from `Timeline` lists of the same form). Follow works unchanged, since it walks the list.
- **Highlight**: a site's cell is framed like a place settlement, and a monster's lair site at Y (`monster_at`) likewise.
- **Kind groups**: done (r2). The `monsters` group is code 5 and `ALL_GROUPS` is 0x3F (4.4).
- **Figures** (P5-06, type 6) and factions (7) attach the same way.

Nothing in Part A or B may assume that the entity list holds civilizations only.

## 10. Library tests (Part A, test writer; independent reference)

### 10.1 Reference and golden

The reference is `worldgen/tests/reference/p4_browser_reference.py`: Python 3, exact integers, `sqlite3` only. It may import `p3_report_reference.py` (sentences, notes, digest, blocks) and `p4_markers_reference.py` (places), and may add span output to the former's renderer if that leaves its own outputs unchanged. It does not port the C++. The golden is `worldgen/tests/golden/p4-05-browser.txt`, written by `python3 -B worldgen/tests/reference/p4_browser_reference.py golden`, with header `# ` lines. Its cases are those of P4-04 10.3 (world cases, `sample-42-1024`, the P3-07 fixtures with history, T2) plus the new fixture **T3** `t3-browser`. Per case, `<case> <name> <sha256>` of these LF dumps:

| Name | Dump lines |
| --- | --- |
| `civilizations` | `civ <id> <race> <founded> <ended> <ending_event> <events> <name>\|<race_name>` |
| `links` | per event, ascending: `event <id> <text>`, then `link <start> <length> <type> <id>` per link |
| `details` | per event: `detail <id> <group word> <summary 0\|1> <kind_key>`, `note <text>`, `part <role> <type> <id> <name>`, `value <key> <v>`, `cause <id>`, `effect <id>`, `place <x> <y>` or `place none` |
| `blocks-summary`, `blocks-full` | per civilization: `block <id>`, then per line `line <text>` and `link <start> <length> <type> <id> <tagged 0\|1>` |
| `filters` | `filter <description>` then the ids, one line, for: all × {summary, full}; each civilization × {summary, full}; each settlement and each region with at least one event, full; each group, full; spans [0, E/2] and [E/2, E] full; civilization 1 ∧ group war, full |
| `search` | `query <q>` then `match <type> <id> <name>` for q in {`a`, `the`, `THE`, each civilization's first three bytes, a query matching nothing} with limit 50, and `a` with limit 3 |

A `groups <sha256>` line covers `group <key> <word>` for every compiled kind key plus `unknown_key`. Text in dumps is the raw bytes, with only `\`, LF and bytes below 0x20 escaped as P3-07's `escape_history_text`.

The 1024 cases run in `worldgen-tests-slow-browser`. The golden is re-recorded only by the reference.

### 10.2 Unit tests (`worldgen-tests-browser`)

1. `render_event_linked`: its text equals `render_event` for every event of every golden case. On T3, links are checked for `{role}`, `{role:all}` with three participants, `{role:plural}` and `{role:adjective}`, a lower-cased `The `, a region renamed after the event (former name), `{settlement}`, and a name with non-ASCII bytes (offsets in bytes). Links never overlap and ascend.
2. `event_group` for every key of the table, every one of the 55 compiled kinds (none falls to `other`), `""` and `"warx"`.
3. Block lines: `block_line_report_text` over `civilization_block` equals the block lines of `build_history_report` at both levels for every golden case. This holds after the refactor too, and the P3-07 goldens are the independent guard. Display and report forms agree under B4's transform. A civilization with no capital, an ended one, a seceded one and one with an adversary of another type appear on T3.
4. `select`: every filter rule of 4.5 on T3 (span overlap at both ends, multi-year events, two entity fields together, `summary_only`), and FatalErrors.
5. `search`: case folding A–Z only (a non-ASCII byte is not folded); a region found by a former name; the limit; an empty query.
6. `event_place` equals the places of `markers_at` for every event of every golden world case.
7. `BrowserIndex::build` on a world whose history loads: no error; the out-of-memory path is not reachable in tests.

**T3** (`worldgen/tests/data/browser/`, built through the writer API and through SQL as P4-01 13.3) has three civilizations of two races (one ended by conquest, one seceded), a renamed region, a settlement with a non-ASCII name, an event with three `parties`, a cause chain three deep, an event with two effects, and a multi-year event.

## 11. Invariants

1. The sentence of `row_text` and of `event_detail` is `render_event` byte for byte, and the note is `history_note(view, e, 0)`.
2. The report form of `civilization_block` is the report's block, and the report bytes are unchanged by this stage.
3. `select` results are ascending and are a function of (world, filter).
4. Follow visits exactly the current list, in order, and never changes the filter.
5. A year change rebuilds the event list only with This year on, and only when its ids change.
6. Nothing in `gdext/` parses report text outside the headless check, issues SQL on a history table or tests a kind by id (V3, V7).

## 12. Carried-over viewer items

### 12.1 World critic's review of P4-04 (`docs/reviews/p4-04-markers-world-critic.md`)

| Finding | Decision | What changes |
| --- | --- | --- |
| **F1** markers too small at the fitted zoom | **Adopt** (viewer). Glyph cell g = 1 for z < 0.5, **2 for 0.5 ≤ z < 10**, 3 for z ≥ 10 (P4-04 8.4 had 1 below z 4). At the fitted view of a 1024 map, a hamlet is 14 px, a village 18, a city 22 and a capital town 22 with a 4 px outline, which brings out the capital outline and the declining interior, the critic's two cases. No separate capital shape: with g = 2 the 2-cell outline reads (the critic's own x2 images), and the world critic re-judges on P4-05's screenshots | `layer_map_view.cpp` constants; the hit-test uses the same g. No library change |
| **F2** city and town alike | **Defer**. After F1 a city is 22 px against a town's 18 px with a 6 × 6 px ink centre, and the hover and the browser name the kind. A glyph change would move every overlay golden for a doubtful gain. Re-judged on P4-05's screenshots | none |
| **F3** ruins faint; the faded X lost on light fills | **Adopt** (library). Ruin (base 8): radius 4, ink where max(\|dx\|, \|dy\|) ≤ 3 and \|\|dx\| − \|dy\|\| ≤ 1, then the halo rule (44 halo, 33 ink). Faded (base 9): radius 3, fill `MARKER_FADED` where \|dx\| = \|dy\| ≤ 2, then a halo on every 0 cell with an 8-neighbour of class 2 (36 halo, 9 fill). Drawings below | `marker_glyph`; P4-04 5.3 is replaced by this row |
| **F4** owner fill hard to see inside its realm | **Adapt**. No glyph change. The browser's civilization list shows each realm's swatch next to its name, which explains the fills, and the civilization frames of 6.2 mark a realm's settlements | none beyond section 6 |
| **F5** siege and plague rings alike | **Adopt** (library). Plague ring colour (150, 210, 0) becomes **(0, 190, 120)**, a clear green against siege orange (255, 150, 0), battle red and sack magenta | `event_symbol_colour(plague)` |
| **F6** hover far from the cursor; small hit area | **Adopt** (viewer). Entries with `small`, and passes, are also hit when the mouse is within one glyph cell (Chebyshev) of a non-zero cell, tested in the same reverse order. A ruin or faded ruin with ruin event 0 reads `…, ruined in <R>: cause not recorded`. `<since>` in P4-04 8.5's example is a placeholder (the viewer writes the year), so nothing changes there. The text stays in `%CellLabel`. A click now selects the settlement in the browser, whose detail shows the same text beside the map | `layer_map_view.cpp`, `describe_marker`; M7 text |
| **F7** key-list rows | **Adopt** (viewer). Key icons are drawn at g = 2 except the ring rows (g = 1; a ring at g = 2 would be 46 px), so the pass chevron is 14 px. The `other event` label becomes `any other event` | `get_marker_legend()`, `viewer_screen.cpp`; M1 key list |

F3 drawings (`h` halo, `F` fill, `K` ink, `·` none):

```
ruin (r 4)     faded (r 3)
hhhh·hhhh      hhh·hhh
hKKhhhKKh      hFhhhFh
hKKKhKKKh      hhFhFhh
hhKKKKKhh      ·hhFhh·
·hhKKKhh·      hhFhFhh
hhKKKKKhh      hFhhhFh
hKKKhKKKh      hhh·hhh
hKKhhhKKh
hhhh·hhhh
```

The ring's colour band (P4-04 5.4) still lies outside every glyph body. A ruin under a ring is drawn after it (P4-04 6.1).

### 12.2 What moves (F3 and F5, Part A)

| Output | Change |
| --- | --- |
| `p4-04-markers.txt` | `glyphs` (F3). The `overlay` and `overlay-scale3` lines of every case with a ruin, faded ruin or plague ring at that year (in practice the E and E/2 lines of most world cases; the T2 lines with ruins). `static`, `markers`, `markers-years` and `table` lines are **unchanged** (no marker set changes). Re-recorded by the reference, after `p4_markers_reference.py` takes the new grids and colour |
| `<preset>-<case>.markers.txt` (`world-hashes.sh`) | the hashes of years with a ruin, faded ruin or plague ring; compared across presets as before |
| `worldgen/tests/markers_*` | the 5.3 grids and counts, the plague colour |
| CLI `render --markers` images | ruins, faded ruins and plague rings |
| World files, every other golden, `report`, `hash`, `territory.txt`, `history.*.txt` | none |

`GENERATOR_VERSION` is not bumped (no world byte, no generator vector). P4-04 5.3 and 3.4 get a pointer line to this section at merge.

### 12.3 The slow year change (first task of Part B)

P4-04 8.4 measured the slowest year change at 170 ms (to year 5494, seed 42 at 1024, `linux-gcc-release`), against P4-03's 50 ms. The territory redraw alone is 3.9 ms in the library (P4-01 6.2), so the time is elsewhere. **Investigation, before any browser code:**

1. Extend `--bench` to time each step of one year change separately: (a) `year_state` and holders, (b) `render_territory` into the image, (c) the `view_image_changed` handler (`ImageTexture::update` and anything else connected), (d) the marker step, (e) the model's legend (`HistoryState::legend`, `legend_array`), (f) the screen's legend rebuild (`show_legend`, icon textures), (g) the year bar's labels and buttons, and (h) the other handlers of `year_changed`. Print the maximum and mean of each over 0 … E and back, and the year of each maximum.
2. Run it twice: headless (`--headless --bench`, the dummy renderer, where texture uploads cost nothing) and with a display (`xvfb-run` or the desktop, a windowed `--bench`). Also measure, in the windowed run, the time from `set_year` to the next `RenderingServer` `frame_post_draw`.
3. Record the table in the validation note, and say which step makes the 170 ms and why year 5494 is the worst (expected: many alive civilizations, so the legend rebuild with a new `ImageTexture` per row, or a texture re-created rather than updated).

**Bound to meet**: on `linux-gcc-release`, seed 42 at 1024, windowed: every year change to the next drawn frame within **50 ms**, and the model part (a, b, d, e) within **15 ms**, with the P5 world sizes of the time. Remedies are the engineer's choice and are presentation-side only. Likely ones: rebuild the legend only when the list of alive civilizations or their rows differ (an answer compared, not cached, as the markers do, V6); one icon texture per distinct colour set, kept for the world; `ImageTexture::update` on the existing texture, never a new one. The P4-03 and P4-04 checks must stay green. The browser's budget in section 7 is measured on top of the fixed year change.

## 13. Amendments (applied by the lead at merge unless done here)

- **P4-00 3.1 P4-05 paragraph**: P4-05 adds to `worldgen` (4.1 to 4.6) and the report prints its blocks from the shared lines. P4-00 7 row P4-05 is implemented as B1 to B6.
- **P4-04**: 5.3 (ruin, faded) and 3.4 (plague colour) replaced by 12.1; 8.4 g steps by F1; 8.5 the hit margin and the `cause not recorded` text; 8.6 the icon scale and the `any other event` label; 4 the `event_place` member.
- **P4-03 4.3**: the keys of 6.5 added; 4.1 the third pane.
- **CLAUDE.md, README** (lead): `worldgen/inspect/history_browser.hpp`, the CTest entries `worldgen-tests-browser` and `worldgen-tests-slow-browser`, the golden file.
- **Proposals**: entry "Viewer v1, P4-05: the report prints its civilization blocks from the lines the browser shows" (added with this spec).

## 14. Open questions for Paul

Each has a recommendation and the interim choice the implementation takes.

1. **Where the browser sits.** Recommendation and interim: a right-hand pane with tabs Browser and Legend (6.1), with B to hide it. It costs the map about 6 % of its fitted size at 1600 × 1000, because the map was height-bound. Alternative: a fourth column beside the legend, which leaves a 1024 map about 600 px wide.
2. **Follow walks the filtered list.** Recommendation and interim: follow steps through the event list as filtered (5.3), so "follow a realm's wars" is the group filter plus follow. Alternative: follow always walks every event of the civilization whatever the filter.
3. **One level for list and block.** Recommendation and interim: "All events" switches both the list and the block to the full report, so the browser always shows one of the two reports (6.4). Alternative: separate toggles.
4. **Highlight by dimming.** Recommendation and interim: the land a selected realm does not hold is dimmed (6.2). An outline of one cell is invisible at the fitted zoom. Alternative: a coloured outline drawn at screen width.
5. **Glyph changes now.** Recommendation and interim: adopt F3 (heavier ruins, haloed faded ruins) and F5 (green plague) in Part A, which re-records `p4-04-markers.txt` and moves `markers.txt` hashes; defer F2 (city and town) to the screenshot review after F1. Alternative: defer every glyph change and leave P4-04's goldens untouched.
6. **A click on the map selects.** Recommendation and interim: a plain left click without drag selects (5.5), since drag pans and double click fits. Alternative: select only with a modifier key.

## 15. Readings chosen

1. "Its block as the history report shows it" means the report's own lines, without escapes and tags and with links, at the browser's level (4.2, 6.4).
2. "The summary set" is the report's digest set, through `history_report_digest`, so that it follows P5-02's second choice (4.3).
3. Event kind groups are a library table by key (4.4). The design names no groups, and six cover the 55 kinds (r2: `monsters` added for P5-02's kinds).
4. Events at a place are the timeline's lists for a settlement or region (P4-01), selected from the map by marker or by region cell (5.5).
5. The browser's link targets use the report's tags (`c3`, `e57`), one encoding for the viewer, the check and the golden.

## 16. Open items (r2)

From the determinism audit of Part A (`docs/reviews/p4-05-browser-library-audit.md`), both advisories, neither blocking:

- **A1. Empty name against no name in a block line.** `BlockBuilder::entity` turns a missing name into an empty `Segment::text`, and `block_line_report_text` prints the tag alone for an empty text, where the former `Report::ref` did so only for a missing name. Generated worlds never have an empty civilization, settlement or race name, so no report byte changes; on a damaged file with an empty name the report text differs from the text before the refactor. Remedy if wanted: a `has_name` flag in `Segment`, or empty names refused on load as `is_region_name_text` does for regions.
- **A2. `search` relies on the `region_name` rows being ordered by region.** `std::lower_bound` over `region_names()` holds because the rows are read in primary-key order; the P5-12 audit's A1 (no check that the rows ascend) is the related open item. Viewer-only output, not in the cross-platform contract.

## Revision notes

- **r2** (2026-11-23), text only, from what was built and ruled; no change to code or output. 4.4: group `monsters` = 5 for kinds 44 to 55, `ALL_GROUPS` = 0x3F, all 55 kinds covered, `other` holds no shipped kind (lead ruling). 4.2: `history_entity_tag`; an entity without a name displays its tag as an untagged link; the FatalErrors of `civilization_block`. 4.5: the error texts as built. 4.6: `markers: no event <e>`. 4.7: budgets for the 3,000-event world only, the bench suite's real name (`browser-bench`) and the measured costs. 5.2: only `c`, `s`, `rg` and `e` are links in the viewer. 6.1: the group menu from `get_event_groups()`, Follow a check box. 4.3: the P5-02 step done. 7: the large world at `--event-intensity turbulent` (2,646 events). 9: the kind-groups bullet done. 10.2 item 2 and 15.3: 55 kinds. 16: the audit's advisories A1 and A2 as open items.
- **r1** (2026-11-23): first draft.
