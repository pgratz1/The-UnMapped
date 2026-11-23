# P4-04 Settlement and place markers

Status: **r1 draft, 2026-11-23**, for approval by the lead. Phase P4 Viewer v1, stage 4 of the overview `docs/specs/p4-00-viewer-v1-overview.md` (3.1 "P4-04", 3.2, rules V1 to V10, 7 row P4-04, 8, 9). Builds on P4-01 r3 (`docs/specs/p4-01-timeline.md`: `Timeline`, `SettlementState`, `YearState`, `load_history`), P4-03 r4 (`docs/specs/p4-03-history-view.md`: the year bar, `HistoryState`, the territory view, the headless check H1 to H9) and P4-03a (`docs/specs/p4-03a-territory-colour-followups.md`: the timeline form of `render_territory`, `Cell::half`, legend tooltips), as on branch `p4/03a-final`. Design doc 10 ("World viewer": "settlements and sites as markers") and 13 (P4 row: "settlement and site markers").

Numbers for the 512 world are from `worldgen-cli generate --seed 42 --map-size 512 --data game/data` at generator 0.22.0 (the headless check's real world): E = 6000, 67 settlements (at E 43 thriving, 7 declining, 9 ruined, 8 faded), first ruin in year 4984, first `ruin_faded` event in 3992, 1 pass, at most 6 events with a place in one year. 1024 worlds, seeds 0 / 5 / 42: 88 / 115 / 96 settlements, 2 / 2 / 1 passes, at most 7 / 6 / 8 placed events in a year; settlements standing at the same time are 23 cells or more apart (median nearest neighbour 31; the exceptions are relaxed awakening sites and a faded ruin sharing its cell with a later settlement, P3-05 invariant 2). Measured with `~/.cache/the-unmapped/out/p4-04-spec/`.

**No world byte changes.** Generator stays **0.22.0**, schema stays **8**. New: golden file `worldgen/tests/golden/p4-04-markers.txt`, the CLI options of section 7 (P0-04 amendment 24), and a `markers.txt` file kind in `world-hashes.sh`.

## 1. Purpose

After this stage the map in the viewer shows, above whichever view is selected, every settlement that exists in the year shown: its shape by kind, filled with its owner's colour, hollow while declining, a thick outline for a capital, a cross for a ruin, a faint cross for a faded ruin when asked for; a coloured ring around the place of each event of that year (red for a battle, magenta for a sack or capture, orange for a siege, green for a plague, white for anything else); and the mountain passes. Hovering a marker names it and says what it was in that year. A sidebar group turns the overlay and its parts on and off and shows the key.

**Rule: the library answers the year, the viewer draws it** (P4-00 V3, proposals "Viewer v1: the library answers the year, the viewer draws it"). Which markers exist at Y, where, with which glyph and colour, which settlement is a capital, when and by what a ruin fell, and the pixels of every glyph are `worldgen` (section 4 to 6). The extension draws those glyphs at screen size and handles the toggles and hover (section 8). `worldgen-cli render --markers` draws the same overlay on any image, so marker images can be made and hashed without the viewer, and the headless check compares the viewer's marker set with the CLI's pixel for pixel, as it does for `territory`.

This amends P4-00 3.1, which had P4-04 add nothing to `worldgen` unless the marker list was wanted in the CLI; it is (section 15).

## 2. What this stage touches, and the two parts

| Part | Area | Files | Owner |
| --- | --- | --- | --- |
| **A library** | markers | new `worldgen/include/worldgen/inspect/markers.hpp`, `worldgen/src/inspect/markers.cpp` | generator engineer |
| A | words | `worldgen/include/worldgen/history/digest.hpp`, `digest_tables.cpp` (or `digest.cpp`): `settlement_kind_word`, `settlement_status_word` moved from `history_report.cpp` (its bytes unchanged) | generator engineer |
| A | CLI | `tools/worldgen-cli/cli.cpp`: `--markers`, `--faded`, `--year` (section 7) | generator engineer |
| A | tests | `worldgen/tests/markers_*.cpp` (SHARD `markers` pattern `*markers_*`, SLOW `slow-markers`), `tools/worldgen-cli/tests/cli_markers_test.cpp` (into the `-territory` group), fixture `worldgen/tests/data/markers/`, reference `worldgen/tests/reference/p4_markers_reference.py`, golden `worldgen/tests/golden/p4-04-markers.txt` | test writer |
| A | CI | `tools/ci/world-hashes.sh`: file kind `markers.txt` (section 11) | build engineer |
| **B viewer** | model | `gdext/src/viewer/history_state.*`, `viewer_model.*` | viewer engineer |
| B | screen | `gdext/src/viewer/layer_map_view.*`, `viewer_screen.*`, `game/viewer/viewer.tscn` (layout only) | viewer engineer |
| B | check | `gdext/src/viewer/viewer_check.*`, `gdext/cmake/headless_check.cmake`; `gdext/README.md` | viewer engineer |

Part A goes through the generator engineer, test writer, determinism auditor and validation specialist and merges first. Part B follows on its own branch (viewer engineer, headless check on both extension jobs, validation specialist); it may start against the header of section 4 at once, and only its pixel checks (M6) wait for Part A's merge. The world critic reviews screenshots of Part B (design 13: it changes how history reads), together with P4-05's if they land close together.

Not touched: every world-writing path, `report` and `hash` text, the `Composite` enum, `render_territory`, the existing view images (markers are never drawn into a view's image in the viewer; section 8.4).

## 3. Definitions at year Y

E = `meta.history_end_year`, Y in [0, E], the state at the end of year Y (P4-01 4). Every definition reads only what `load_history` has checked, plus tables `region` and `pass` (4.3).

### 3.1 Settlement entries

One entry per settlement s with `SettlementState(s, Y).exists` (`founded_year` ≤ Y), taking `owner`, `kind`, `status`, `population`, `since` from the state (the census row of largest year ≤ Y: population is the last recorded, P4-01 4). Entries with status 4 (faded) are left out unless `MarkerOptions::faded`. Its cell is the `settlement` row's `x`, `y`.

| Status at Y | Entry type | Glyph |
| --- | --- | --- |
| 1 thriving | `standing` | kind shape, filled (5.2) |
| 2 declining | `standing` | kind shape, hollow (5.2) |
| 3 ruined | `ruin` | ruin cross (5.3) |
| 4 faded | `faded` (only with the option) | faded cross (5.3) |

### 3.2 Capital at Y

The capital is not stored by year (`civilization.capital` is the end state), so the library derives it from the events that set it (the three `set_capital` sites of P3-05 and P3-06):

| Kind key | Role naming the civilization c | The capital |
| --- | --- | --- |
| `settlement_founded` | `founder` | the event's `settlement` |
| `civ_seceded` | `civilization` | the event's `settlement` |
| `capital_moved` | `civilization` | the event's `settlement` |

Kinds and roles are found by key and role name in the stored catalogue (V7); a kind the world lacks contributes nothing. The **capital run** of c is the list of (year, event, settlement) of those events with a participant of that role, entity type 3 and id c, and `settlement` ≠ 0, in ascending event id. **capital(c, Y)** = the settlement of the last entry with year ≤ Y, if c is alive at Y (`YearState::civilizations`), that settlement exists and is standing (status 1 or 2) at Y and its owner at Y is c; otherwise none. A standing entry is a **capital** iff it is capital(owner, Y).

Measured on the four worlds above, at every year: every civilization alive at Y has a capital by this rule, standing and its own, and at E it equals `civilization.capital` for every civilization (26,902 civilization-years at seed 42 / 512). Invariant 4 (section 12) makes the test writer's sweep hold the rule to that; a P5 change that sets a capital by another event kind must add its row here and in the library (the invariant fails otherwise).

### 3.3 Ruin year and ruin event

`settlement.ruined_year` and `ruin_event` are NULL and 0 once a ruin has faded (P3-04 9.2), so the library derives both per settlement, once:

- **ruined year** R(s) = the `year` of the first census row of s with status 3; none if it has none.
- **ruin event** = `settlement.ruin_event` when non-zero; otherwise the smallest cause of the event of kind key `ruin_faded` whose `settlement` is s (P3-05: a faded ruin is caused by its ruin event); otherwise 0.

A `ruin` or `faded` entry carries R(s) and the ruin event (a standing entry: −1 and 0). Measured: for every ruined settlement R(s) = `ruined_year`, and for every faded one the year of the derived event is R(s) (invariant 5).

### 3.4 Event places

For each event e of Y (`YearState::events`: `year` ≤ Y ≤ `end_year`, ascending id) with a place: the cell of `settlement` (the settlement's `x`, `y`, whatever its state) when `settlement` ≠ 0, else the region's seed cell (`region.seed_x`, `seed_y`) when `region` ≠ 0, else no place. Events with the same cell form one **place**, listed in ascending order of its smallest event id, holding its event ids ascending. A place's **symbol** is the highest of its events' symbols in the order sack > siege > battle > plague > plain, each event's symbol given by its kind key (V7):

| Symbol | Kind keys | Ring colour | Legend label |
| --- | --- | --- | --- |
| `sack` | `sacked`, `settlement_captured` | (200, 0, 200) | `sack or capture` |
| `siege` | `siege`, `refuge_held` | (255, 150, 0) | `siege` |
| `battle` | `battle`, `battle_at_pass` | (230, 30, 30) | `battle` |
| `plague` | `plague`, `plague_outbreak`, `plague_spread` | (150, 210, 0) | `plague` |
| `plain` | any other key | (255, 255, 255) | `other event` |

Places are left out unless `MarkerOptions::events`. (P4-06 flashes these places; P4-05 highlights the place of a selected event with the same cell rule.)

### 3.5 Passes

One entry per row of table `pass`, at (`col_x`, `col_y`), with its `kind` (1 lowered pass, 0 natural gap); independent of Y; left out unless `MarkerOptions::passes`. Passes are geography (P1-01), shown here because design 5 makes passes sites.

### 3.6 Sites and figures: reserved, not built

P5-02 adds table `site` (entity type 9) and P5-06 figures (type 6). This stage reserves, and builds nothing for them:

- entry type codes 6 `site` and 7 `figure` in `MarkerType` (section 4), never produced now;
- the draw position between ruins and standing settlements (6.1), and a glyph base code range 16 to 31 for site kinds;
- the rule that a site marker's existence at Y, its cell and its glyph come from the library through `markers_at`, like every other entry, so the viewer gains sites by a rebuild and one glyph table row, not a new query.

Until then, "site markers" are ruins, passes and event places (P4-00 3.1).

## 4. Library API (`worldgen/inspect/markers.hpp`)

```cpp
namespace worldgen::inspect {

enum class MarkerType : std::uint8_t { pass = 1, place = 2, faded = 3, ruin = 4, standing = 5,
                                       site = 6 /*reserved*/, figure = 7 /*reserved*/ };
enum class EventSymbol : std::uint8_t { plain = 0, plague = 1, battle = 2, siege = 3, sack = 4 };  // order = priority

struct MarkerOptions { bool faded = false; bool events = true; bool passes = true; };

struct SettlementMarker {
    std::int64_t settlement = 0, x = 0, y = 0;
    std::int64_t kind = 0, status = 0, owner = 0, population = 0, since = 0, founded = 0;
    bool capital = false;                          // standing only (3.2)
    std::int64_t ruined_year = -1, ruin_event = 0; // ruin and faded only (3.3); standing: -1 and 0
    friend bool operator==(const SettlementMarker&, const SettlementMarker&) = default;
};
struct PlaceMarker {
    std::int64_t x = 0, y = 0;
    EventSymbol symbol = EventSymbol::plain;
    std::vector<std::int64_t> events;              // ascending id, at least one
    friend bool operator==(const PlaceMarker&, const PlaceMarker&) = default;
};
struct PassMarker {
    std::int64_t pass = 0, x = 0, y = 0, kind = 0;
    friend bool operator==(const PassMarker&, const PassMarker&) = default;
};
struct YearMarkers {
    std::int64_t year = 0;
    std::vector<SettlementMarker> settlements;     // ascending settlement id (3.1)
    std::vector<PlaceMarker> places;               // ascending smallest event id (3.4)
    std::vector<PassMarker> passes;                // ascending pass id (3.5)
    friend bool operator==(const YearMarkers&, const YearMarkers&) = default;
};

[[nodiscard]] EventSymbol event_symbol(std::string_view kind_key) noexcept;   // 3.4, keys as bytes
[[nodiscard]] std::string_view event_symbol_word(EventSymbol) noexcept;       // "plain" ... "sack"
[[nodiscard]] Rgb event_symbol_colour(EventSymbol) noexcept;

class MarkerIndex {
public:
    // Reads tables region (seed cells) and pass, and builds the capital runs (3.2) and the ruin
    // years and events (3.3) from the history. Errors, first returned: the reader's; table pass
    // with the report's row checks and message (P1-00 10.3, P0-04 amendment 17: corrupt_layer
    // "corrupt table 'pass': <reason>", map size from meta.param.map_size); region ids not equal,
    // in order, to history.tables.regions: corrupt_layer "corrupt table 'region': the file changed
    // while it was read". std::bad_alloc: out_of_memory "out of memory while building the markers".
    // Never throws for a bad file. Holds no reference into `history` or `world`.
    [[nodiscard]] static db::Result<MarkerIndex> load(const db::WorldReader& world,
                                                      const history::LoadedHistory& history);

    // The markers of state.year (section 3). `state` must be history.timeline's year_state of that
    // year and `history` the one load was given (FatalError "markers: ..." otherwise, where
    // detectable: settlement count or year out of range). Reuses out's buffers.
    void markers_at(const history::LoadedHistory& history, const history::YearState& state,
                    const MarkerOptions& options, YearMarkers& out) const;
    [[nodiscard]] YearMarkers markers_at(const history::LoadedHistory& history, std::int64_t year,
                                         const MarkerOptions& options) const;   // FatalError as Timeline for a bad year

    // 3.2 and 3.3 directly (FatalError "markers: no civilization <c>" / "no settlement <s>", or a bad year).
    [[nodiscard]] std::int64_t capital(const history::LoadedHistory& history, std::int64_t civ, std::int64_t year) const;  // 0: none
    [[nodiscard]] std::int64_t ruined_year(std::int64_t settlement) const;   // -1: never ruined
    [[nodiscard]] std::int64_t ruin_event(std::int64_t settlement) const;    // 0: none
    [[nodiscard]] std::span<const db::PassRow> passes() const noexcept;      // ascending id
};

// Glyphs (section 5) and drawing (section 6).
struct GlyphKey {
    std::uint8_t base = 0;        // 1..5 settlement kind, 8 ruin, 9 faded, 10 pass, 11 place ring; 16..31 reserved for sites
    bool declining = false;       // base 1..5 only
    bool capital = false;         // base 1..5 only
    friend bool operator==(const GlyphKey&, const GlyphKey&) = default;
};
struct Glyph {
    std::int32_t radius = 0;                 // the grid is (2 radius + 1) squared
    std::vector<std::uint8_t> cells;         // row-major from (-radius, -radius); 0 none, 1 halo, 2 fill, 3 ink, 4 interior
};
inline constexpr Rgb MARKER_INK{0, 0, 0};
inline constexpr Rgb MARKER_HALO{240, 240, 240};
inline constexpr Rgb MARKER_FADED{150, 150, 150};
inline constexpr Rgb MARKER_NO_OWNER{128, 128, 128};
[[nodiscard]] Glyph marker_glyph(GlyphKey key);          // FatalError for a key section 5 does not define
[[nodiscard]] GlyphKey glyph_of(const SettlementMarker&) noexcept;

// Draws the markers onto an RGB buffer laid out as render_rgb (width x height cells at scale k),
// in the order of 6.1. colours: civilization_colours (index id - 1). FatalError "draw_markers: ..."
// when rgb.size() != 3 (width k) (height k), k outside 1..16, or an owner without a colour.
void draw_markers(std::span<std::uint8_t> rgb, std::uint32_t width, std::uint32_t height, std::uint32_t scale,
                  const YearMarkers& markers, std::span<const Rgb> colours);
}
```

In `worldgen/history/digest.hpp`: `std::string_view settlement_kind_word(std::int64_t kind) noexcept` (1 `hamlet` … 5 `fortress`) and `std::string_view settlement_status_word(std::int64_t status) noexcept` (1 `thriving` … 4 `faded`), empty for any other value: the report's `SETTLEMENT_KIND` and `STATUS` tables moved, as P4-03 9.5 moved `phase_kind_word`; `history_report.cpp` calls them and its bytes are unchanged.

**Cost.** `load`: one pass over events and participants, census and the two tables, O(n). `markers_at`: O(S + C log k + m + P) for S settlements, C civilizations (one binary search in each capital run), m events of Y, P passes, plus the place grouping (at most 8 placed events a year: a linear merge by cell is enough; any method whose result is the order of 3.4). Expected under 0.2 ms at 1024; budget **1 ms at 1024, 4 ms at 2048** (on top of P4-00 8's 2 ms year answer). `draw_markers`: at most 23 × 23 glyph cells per marker, k² pixels each.

## 5. Glyphs

### 5.1 Shape predicates (integers, offsets dx, dy from the marker's cell; y grows southward)

| | Predicate |
| --- | --- |
| C(r) disc | dx² + dy² ≤ r² + r |
| S(r) square | max(\|dx\|, \|dy\|) ≤ r |
| D(r) diamond | \|dx\| + \|dy\| ≤ r |

C(0), S(0) and D(0) are the centre cell only.

**Halo rule** (every glyph except `faded`): after the other classes are set, every grid cell still 0 with an 8-neighbour of class 3 becomes 1.

### 5.2 Standing settlements (base = kind)

| Kind | P | r |
| --- | --- | --- |
| 1 hamlet | C | 1 |
| 2 village | C | 2 |
| 3 town | S | 2 |
| 4 city | S | 3 |
| 5 fortress | D | 3 |

With o = r + 1 + (1 if capital), for every cell with Chebyshev distance ≤ o + 1 (that is the radius): in P(r): 4 if declining and in P(r − 1), else 2; otherwise in P(o): 3. City: every cell of S(1) becomes 3. Then the halo rule. So a capital has a two-cell outline and a declining settlement is a ring of its colour around an interior that shows the map. Fill colour: `colours[owner − 1]`; owner 0 (not on checked data) `MARKER_NO_OWNER`.

Reference drawings (`h` halo, `F` fill, `K` ink, `.` interior, `·` none), derived from the rule and checked by the tests:

```
hamlet (r 3)     village (r 4)     town (r 4)        city (r 5)          fortress (r 5)
·hhhhh·          ··hhhhh··         hhhhhhhhh         hhhhhhhhhhh         ····hhh····
hhKKKhh          ·hhKKKhh·         hKKKKKKKh         hKKKKKKKKKh         ···hhKhh···
hKFFFKh          hhKFFFKhh         hKFFFFFKh         hKFFFFFFFKh         ··hhKFKhh··
hKFFFKh          hKFFFFFKh         hKFFFFFKh         hKFFFFFFFKh         ·hhKFFFKhh·
hKFFFKh          hKFFFFFKh         hKFFFFFKh         hKFFKKKFFKh         hhKFFFFFKhh
hhKKKhh          hKFFFFFKh         hKFFFFFKh         hKFFKKKFFKh         hKFFFFFFFKh
·hhhhh·          hhKFFFKhh         hKFFFFFKh         hKFFKKKFFKh         hhKFFFFFKhh
                 ·hhKKKhh·         hKKKKKKKh         hKFFFFFFFKh         ·hhKFFFKhh·
                 ··hhhhh··         hhhhhhhhh         hKFFFFFFFKh         ··hhKFKhh··
                                                     hKKKKKKKKKh         ···hhKhh···
                                                     hhhhhhhhhhh         ····hhh····

village declining (r 4)    town capital (r 5)
··hhhhh··                  hhhhhhhhhhh
·hhKKKhh·                  hKKKKKKKKKh
hhKFFFKhh                  hKKKKKKKKKh
hKF...FKh                  hKKFFFFFKKh
hKF...FKh                  hKKFFFFFKKh
hKF...FKh                  hKKFFFFFKKh
hhKFFFKhh                  hKKFFFFFKKh
·hhKKKhh·                  hKKFFFFFKKh
··hhhhh··                  hKKKKKKKKKh
                           hKKKKKKKKKh
                           hhhhhhhhhhh
```

Class counts (halo, fill, ink, interior): hamlet 24, 9, 12, 0; village 32, 21, 16, 0; town 32, 25, 24, 0; city 40, 40, 41, 0; fortress 40, 25, 16, 0; village declining 32, 12, 16, 9; town capital 40, 25, 56, 0.

### 5.3 Ruin, faded ruin, pass

| Base | Rule | Radius |
| --- | --- | --- |
| 8 ruin | 3 where \|dx\| = \|dy\| ≤ 2; halo | 3 |
| 9 faded | 2 where \|dx\| = \|dy\| ≤ 2 (fill `MARKER_FADED`); no halo | 2 |
| 10 pass | 3 where \|dx\| ≤ 2 and dy = \|dx\| − 1 (a chevron pointing north); halo | 3 |

```
ruin        faded     pass
hhh·hhh     F···F     ·······
hKhhhKh     ·F·F·     ··hhh··
hhKhKhh     ··F··     ·hhKhh·
·hhKhh·     ·F·F·     hhKhKhh
hhKhKhh     F···F     hKhhhKh
hKhhhKh               hhh·hhh
hhh·hhh               ·······
```

Counts: ruin 36 halo, 9 ink; faded 9 fill; pass 23 halo, 5 ink.

### 5.4 Place ring (base 11)

3 on C(10) \ C(9) and on C(8) \ C(7); 2 on C(9) \ C(8), filled with the place's symbol colour; the inside C(7) stays 0 (the settlement under it shows and is hit, 8.5); halo rule; radius 11. Counts: 144 halo, 68 fill, 104 ink. The colour band (C(9) \ C(8), from 73 to 90 in dx² + dy²) lies outside the body of the largest settlement glyph, a capital city (S(5), at most 50, halo S(6) at most 72), so it is never covered (6.1 draws settlements later).

## 6. Drawing (`draw_markers`)

### 6.1 Order

Later glyphs overwrite earlier ones, cell by cell, in this order: (1) passes, ascending id; (2) places, in list order; (3) faded entries, (4) ruins, (5) standing entries that are not capitals, (6) capitals; (3) to (6) each ascending settlement id. (Sites, when they come: between 4 and 5.) Within one glyph the classes are disjoint, so their order does not matter. Overlaps are rare (3.1 spacing) and settled by this order: a capital is never hidden by another settlement, and no ring covers a settlement.

### 6.2 Pixels

For a marker at cell (x, y) and glyph cell (dx, dy) of class c: 1 `MARKER_HALO`, 2 the fill colour (5.2, 5.3, 5.4), 3 `MARKER_INK`; 0 and 4 write nothing. At scale k the cell covers output pixels [(x + dx) k, (x + dx + 1) k) × [(y + dy) k, (y + dy + 1) k); cells outside 0 ≤ x + dx < width, 0 ≤ y + dy < height are skipped. So a scale-k overlay equals the scale-1 overlay scaled, and glyphs are fixed in cells: a city covers 11 × 11 cells at every scale (2 % of a 512 map, 1 % of a 1024 one).

## 7. Command line (P0-04 amendment 24)

```
worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME) [--year Y] [--markers [--faded]]
                    [--out PATH] [--scale N] [--force] [--verbose]
```

- `--markers` draws the markers of year Y (section 6, `MarkerOptions{faded, true, true}`) over the image of `--layer` or `--composite`, any of them, after it is rendered at `--scale`. `--faded` adds faded ruins. `--year Y` now applies to `--composite territory` and to `--markers`; default E for both.
- Default output: `<stem>.<name>-<Y>.markers.png` (`<name>` the layer or composite name; `<stem>.territory-<Y>.markers.png` for territory), with `<stem>.<name>-<Y>.markers-faded.png` for `--faded`. Without `--markers` names are as before.
- Checks, in order (first failure): (1) usage, exit 2: the existing checks; `--faded applies to --markers only`; `--year applies to --composite territory or --markers only` (replaces amendment 23's `--year applies to --composite territory only`); the `--year` value as amendment 23. (2) open the world (3). (3) the layer's or composite's own checks (3). (4) with `--markers` or `territory`: `load_history`, its messages (3, the no-history message of amendment 21 (b)). (5) with `--markers`: `MarkerIndex::load` (3, its messages). (6) the year range (2, amendment 23's message). (7) image size, output location, render, draw, write.
- `render --help`: the synopsis above; after the `--year` lines (now reading `with --composite territory or --markers: the year, 0 to the history's end year`):

```
  --markers           draw the settlements, ruins, event places and passes of the year over the
                      image (needs the history)
  --faded             with --markers: also the faded ruins
```

and in `--out`: `, with -<year>.markers.png for --markers`.

## 8. Viewer (Part B)

### 8.1 Model state (`HistoryState`, `ViewerModel`)

On open, after `load_history` succeeds, `MarkerIndex::load(reader, history)`. Its failure leaves the history loaded and the markers off and disabled; the file label gains `markers: <code>: <message>` (V10). Held across years: the index, the options, the enabled flag (default **on**), the current `YearMarkers`, the glyph cache (8.4). Nothing else is cached across a change of Y (V6).

Year change (P4-03 3.4), new step after its step 3: if markers are enabled, `markers_at(history, state, options, scratch)`; if scratch ≠ current: swap, emit `markers_changed()`. Then P4-03 steps 4 and 5. An option or the enabled flag changing recomputes the same way (disabled: the current set becomes empty) and emits `markers_changed()` if it differs. So dragging over years where no settlement changes state and no placed event starts or ends redraws nothing; settlement states change at about 1,700 census rows in 6,000 years, and placed events in at most a few hundred years.

Bound methods (every one valid without history; answers as stated):

| Method | Returns |
| --- | --- |
| `get_markers_available()` | history loaded and the index loaded |
| `get_markers_enabled()`, `set_markers_enabled(on)` | the flag; set returns false and changes nothing when not available |
| `get_marker_option(name)`, `set_marker_option(name, on)` | `faded`, `events`, `passes`; false for any other name or when not available |
| `get_markers()` | Array of Dictionary in draw order (6.1): `{type, id, x, y, glyph, fill, capital, small, events}`; `type` `pass`, `place`, `faded`, `ruin`, `standing`; `id` the pass or settlement id, for a place its smallest event id; `glyph` the glyph name (8.3); `fill` 0xRRGGBB (−1 for glyphs without fill class); `small` true for hamlets, villages that are not capitals, ruins and faded (8.4); `events` PackedInt64Array (places only). Empty when disabled or not available |
| `get_marker_glyph(name)` | Dictionary `{radius, cells}` (`cells` PackedByteArray of section 5); empty for an unknown name |
| `describe_marker(index)` | the hover text of 8.5 for entry `index` of `get_markers()`; empty out of range |
| `get_marker_legend()` | Array of Dictionary `{label, colours, glyph}`, section 8.6 |

Signal added: `markers_changed()`.

### 8.2 Sidebar (`viewer.tscn`, layout only)

Between `LayerList` and `MetaHeading`, all with `focus_mode` none (P4-03 4.3 focus rule):

```
MarkersHeading   Label "Markers"
%MarkersToggle   CheckButton "Show markers" (pressed)
%MarkFaded       CheckBox "Faded ruins"
%MarkEvents      CheckBox "Event places" (pressed)
%MarkPasses      CheckBox "Passes" (pressed)
%MarkerLegend    ItemList (not selectable, auto height)
```

Every control is disabled when `get_markers_available()` is false; the three boxes also while the toggle is off. Key **M** (in `_unhandled_key_input`, as P4-03 4.3) toggles `set_markers_enabled`. The key list `%MarkerLegend` is separate from the view legend, so the legend of every view stays as P2, P4-02 and P4-03 define it.

### 8.3 Glyph names

`<kind word>` (`hamlet` … `fortress`, `settlement_kind_word`), followed by ` declining` and/or ` capital` in that order when set; `ruin`, `faded`, `pass`; `ring` for a place (the colour is `fill`). The model maps names to `GlyphKey` one to one.

### 8.4 Drawing at screen size (`LayerMapView`)

The screen hands `get_markers()` and the glyphs it names to `LayerMapView` (`set_markers(markers, glyphs)`) on `markers_changed` and on `view_selected`; the map view draws them in its `_draw` after the image, in array order, over every view (V4: Godot drawing, floats allowed). With zoom z (screen pixels per cell) and image offset o:

- glyph cell size **g** = 1 for z < 4, 2 for 4 ≤ z < 10, 3 for z ≥ 10 (screen pixels per glyph cell; markers stay about the same size as the map zooms, unlike the CLI's);
- centre cx = floor(o.x + (x + 0.5) z), cy likewise; glyph cell (dx, dy) is the g × g square at (cx + dx g − floor(g / 2), cy + dy g − floor(g / 2));
- at z < 0.5 (a 2048 map fitted to a window) entries with `small` are not drawn and cannot be hovered; capitals, towns, cities, fortresses, places and passes are.

Each distinct (glyph name, fill, g) is drawn from one `ImageTexture` built from the glyph's cells and kept in a cache (dropped when the world closes), so a frame is one `draw_texture` per marker (about 100 to 150). No markers are drawn into the model's view images, so every existing image comparison of the headless check is unchanged.

**Budgets** (P4-00 8): the marker step of a year change within 1 ms at 1024 (4 ms at 2048); drawing the markers within 2 ms a frame; a year change to the next drawn frame stays within 50 ms (200 ms). `--bench` (P4-03 11.2) adds the slowest and mean marker step, the number of `markers_changed` over 0 … E and back, and the time of one marker draw.

### 8.5 Hover

On mouse motion `LayerMapView` hit-tests the drawn markers in reverse draw order: a marker is hit when the glyph cell under the mouse has a class other than 0 (interior 4 counts; the inside of a ring does not, so the settlement in a ring is hit). It emits `marker_hovered(index)` (−1 when none) beside `cell_hovered`. While a marker is hit, `%CellLabel` shows `describe_marker(index)` instead of the cell description; otherwise the cell description as today. Texts (exact; the check compares them; names as `HistoryView` gives them):

| Entry | Text |
| --- | --- |
| standing | `<name>, <kind word>[ (capital)] of <owner name>, <status word>, population <population> recorded in <since>, founded in <founded>` |
| ruin | `<name>, ruin of a <kind word>, ruined in <R>[: <sentence>]` |
| faded | `<name>, faded ruin of a <kind word>, ruined in <R>[: <sentence>]` |
| place | `In <Y>: <sentence 1>[ <sentence 2> …]`, the sentences of its events in ascending id, separated by one space |
| pass | `pass <id>: <lowered pass | natural gap>, col <col_dm div 10>.<col_dm mod 10> m` |

`<sentence>` is `render_event` of the ruin event (3.3; left out with its `: ` when the event is 0); place sentences are `render_event` of each event. Examples (seed 42 at 512, Y = 6000): `Saelo, village of The Realm of Ithieglie, thriving, population 831 recorded in <since>, founded in 950`; `Iennmo, ruin of a village, ruined in 5617: <sentence of e470>`.

`LayerMapView` also gains, for the check only, `get_marker_screen_position(index)` (the centre (cx, cy), or (−1, −1) when not drawn).

### 8.6 Key list (`get_marker_legend()`)

Rows, in order; `glyph` names a glyph drawn as the row's icon with fill (160, 160, 160), `colours` as in `get_legend()`: `hamlet`, `village`, `town`, `city`, `fortress` (their glyphs); `capital` (`town capital`); `declining` (`village declining`); `ruin` (`ruin`); `faded ruin` (`faded`; only while the `faded` option is on); `sack or capture`, `siege`, `battle`, `plague`, `other event` (glyph `ring`, colours [symbol colour]; only while `events` is on); `pass` (`pass`; only while `passes` is on). Empty when markers are off or not available.

## 9. Headless check (P4-00 7 row P4-04)

### 9.1 Worlds and references (`headless_check.cmake`)

Real world (`--seed 42 --map-size 512`), in addition to P4-03 10.1:

- from `history.txt`: R0 = the smallest `ruined <year>` of the `[present]` settlement lines; D0 = the year of the first chronicle line of kind `ruin_faded`; a world content assert (as P4-03 10.1) that both exist and that `report --db` has `passes` ≥ 1;
- the marker years LM = {0, D0 − 1, D0, R0 − 1, R0, F, floor(E / 2), E}, ascending, each once (seed 42 at 512: {0, 2483, 3000, 3991, 3992, 4983, 4984, 6000});
- for each Y in LM: `render --composite territory --year Y --markers --out <real>/markers-<Y>.png`; also `--markers --faded` at E and at D0 (`markers-faded-<Y>.png`), and `render --layer elevation --year R0 --markers --out <real>/elevation-markers-<R0>.png`;
- the viewer run adds `--expect-marker-years <LM>`.

Pattern and geography-only worlds: no new files.

### 9.2 New checks (`viewer_check.cpp`)

| Id | Check |
| --- | --- |
| M0 | without history: `get_markers_available()` false, `get_markers()` and `get_marker_legend()` empty, every control of 8.2 disabled, key M changes nothing |
| M1 | real world at open: available, enabled, options `faded` off, `events` on, `passes` on (read back and from the boxes' pressed state); the key list equals 8.6 without `faded ruin` |
| M2 | at E with `faded` on: the `faded`, `ruin` and `standing` entries, sorted by id, equal the `[present]` settlement lines `s<id> <name>: <kind>, <status>, population <p>, <owner (c<id>) or no owner>, at <x> <y> …`: id, kind word, status word, population, owner (0 for `no owner`), x, y; with `faded` off the same without the faded lines; the counts by kind and status equal those lines' counts; the entries with `capital` are exactly the `capital: <name> (s<id>)` lines of the `[civilizations]` blocks of civilizations alive at E |
| M3 | ruins: for every `[present]` line with `ruined <R>`: at R − 1 the settlement's entry is `standing`, at R it is `ruin`. For every chronicle line of kind `ruin_faded` in year D with `@s<id>`: at D − 1 the entry is `ruin`; at D it is absent with `faded` off and `faded` with it on |
| M4 | event places: for each Y in LM and each of the first 20 chronicle years with an event that has a place: the union of the `events` of the `place` entries equals the set of chronicle event ids with start ≤ Y ≤ end and an `@s` or `@rg` tag; each place's (x, y) is the `at <x> <y>` of the `[present]` line of its `@s`, or, without `@s`, the seed cell of its `@rg` region (the check reads `read_region_rows` through the library, as H7 calls the library); with `events` off there are no `place` entries |
| M5 | passes: the `pass` entries are as many as `report --db`'s `passes`, at the col cells of the library's `MarkerIndex::passes()`; with `passes` off none |
| M6 | pixels: for each Y in LM (set through the slider), the territory view's image copied and drawn on with `draw_markers(…, model's YearMarkers, colours)` at scale 1 equals `markers-<Y>.png`; likewise `markers-faded-<Y>.png` with `faded` on, and the `elevation` view at R0 equals `elevation-markers-<R0>.png`. This compares the model's marker set with the CLI's through the same rasteriser; and `get_markers()` lists exactly the model's `YearMarkers` in the order of 6.1 |
| M7 | hover: at E, for the first standing entry, the first capital and the first ruin in id order: `describe_marker` equals the text of 8.5 built from the `[present]` and `[civilizations]` lines and the chronicle sentence of the ruin event (`<since>` from the model's entry); then a mouse motion sent to `%MapView` at `get_marker_screen_position(i)` makes `%CellLabel` that text; a motion at the centre of a ring that surrounds a settlement shows the settlement's text; a motion away from every marker restores the cell description |
| M8 | toggles: `set_markers_enabled(false)` empties `get_markers()` and emits `markers_changed` once; key M turns it back on; `faded` on adds exactly the faded entries of Y; each change emits `markers_changed` once and none when nothing differs |
| M9 | rebuilds: stepping `set_year` from 0 to E, `markers_changed` is emitted in exactly the years where `markers_at(Y)` ≠ `markers_at(Y − 1)` (the check computes these from the library) |

P4-03's checks are unchanged (markers are not part of any view image or of `get_legend()`). The check stays one CTest entry under 60 s on `linux-gcc-release`: nine more renders (about half a second) and M9's 6,000 year steps (about one second at 512).

Not covered: legibility of glyphs and ring colours on every realm colour (Paul's walkthrough and the world critic's screenshots), and the zoom rule of 8.4 (judged by hand).

## 10. Library tests (Part A, test writer; independent reference)

Reference `worldgen/tests/reference/p4_markers_reference.py`: Python 3, exact integers, `sqlite3` only; may import `p4_timeline_reference.py` (states, territory pixels) but does not port the C++. It computes: settlement entries, capital runs and capital(c, Y), ruin years and events, places and symbols, passes, the glyph grids from section 5's predicates, and the overlay pixels of section 6 on the territory buffer.

### 10.1 Dumps

LF, single spaces, decimal. **Marker dump** M(Y), all options on:

```
year <Y>
settlement <id> <x> <y> <type> <kind> <status> <owner> <population> <since> <founded> <capital 0|1> <ruined_year> <ruin_event>
place <x> <y> <symbol word> <event ids>
pass <id> <x> <y> <kind>
```

(`<type>` `standing`, `ruin`, `faded`; lines in list order.) **Static dump**: `capital <civ> <year> <event> <settlement>` per capital-run entry (civ ascending, then event), then `ruin <settlement> <ruined_year> <ruin_event>` per settlement ever ruined. **Glyph dump**: for each of the 24 keys (base 1 to 5 × declining × capital, then 8, 9, 10, 11), `glyph <name> <radius>` and one line per grid row of digits.

### 10.2 Unit tests (`worldgen-tests-markers`)

1. Every glyph of section 5 against its grid written in the test (the ten drawings of 5.2 and 5.3, the counts of 5.2 to 5.4; the other variants from the rule); `marker_glyph` FatalError for base 0, 6, 12 and a declining ruin.
2. `event_symbol` for every key of 3.4, `"sackedx"`, the empty key; words and colours.
3. On fixture **T2** (`worldgen/tests/data/markers/`, built twice as P4-01 13.3: through the writer API and through SQL): a settlement before founding (absent), standing, declining, ruined in year R (standing at R − 1, ruin at R), faded in D (ruin at D − 1, absent / faded at D); a faded ruin whose ruin event is found only through `ruin_faded`; an owner change; a capital founded, moved, a secession's capital, a civilization ended (no capital); an event with a settlement, one with only a region, one with neither; two events at one cell with symbols battle and sack (one place, `sack`); a multi-year event in the middle of its span; Y = 0, E − 1, E. Expected entries written in the test.
4. `draw_markers` on a 32 × 24 canvas: each class colour; clipping at all four edges; scale 1 and 3 (scale 3 equals scale 1 scaled); the order of 6.1 where a ring, a ruin and two settlements overlap; FatalErrors of section 4.
5. `MarkerIndex::load`: a malformed pass row (the report's message); region ids differing from the history's; `history_missing` is not reachable (load takes a loaded history).
6. `settlement_kind_word` and `settlement_status_word` for 0 … 6; the P3-07 report goldens still pass.

### 10.3 Golden (`worldgen/tests/golden/p4-04-markers.txt`)

Written by `python3 -B worldgen/tests/reference/p4_markers_reference.py golden`, header `# ` lines (generator 0.22.0, provenance, line forms). Cases as P4-01 13.4 (the seven `geo-*` and `hist-*` world cases, `sample-42-1024`, the P3-07 fixtures that have tables `region` and `pass`, and T2), ascending byte order. Per case:

1. `<case> static <sha256>`;
2. `<case> markers <Y> <sha256>` of M(Y) for Y in {0, floor(E / 2), E − 1, E} ∪ the age start years ∪ the ruined years ∪ the years of `ruin_faded` events, ascending, each once;
3. `<case> markers-years <sha256>` of M(0) … M(E) concatenated;
4. world cases: `<case> overlay <Y> <sha256>`, the RGB buffer of `render_territory` at Y with `draw_markers` (all options on) at scale 1, Y in {the smallest ruined year if any, floor(E / 2), E}; and `<case> overlay-scale3 <E> <sha256>`.

Once: `glyphs <sha256>` of the glyph dump. The 1024 cases run in `worldgen-tests-slow-markers`. Re-recorded only by the reference, never to make a C++ test pass.

### 10.4 Property and bench

Over 512 seeds 0–3 (default) and, slow, 512 seeds 4–19 and 1024 seeds 0–9: invariants 1 to 9 of section 12 by brute force. Bench in the skipped suite `timeline-bench`: `MarkerIndex::load`, `markers_at` for every Y, `draw_markers` at E, 1024 seed 42 (prints only).

### 10.5 CLI (`cli_markers_test.cpp`)

1. `--markers` with `--composite territory`, `regions`, `map` and `--layer elevation`, default year and `--year 0`, `--scale 2`, `--faded`: exit 0, default names of section 7; the PNG's pixels equal the library's `render_*` then `draw_markers`.
2. Usage (exit 2): `--faded` without `--markers`; `--year` with `--composite regions` without `--markers` (new message); `render --help` has the lines of section 7.
3. Exit 3: `--markers` on `--until regions` and `--test-pattern` worlds (the no-history message), a malformed `pass` row.
4. `cli_territory_test.cpp`: the `--year` message updated (amendment 24 (b)).

## 11. Cross-platform comparison (`world-hashes.sh`, build engineer)

For each case with history, a new file `<preset>-<case>.markers.txt`: lines `<year> <sha256>`, the SHA-256 of the PNG of `render --composite territory --year <Y> --markers --faded`, at the years of the territory file (0, floor(E / 2), E). `compare` adds the kind `markers.txt` to the kinds it compares and names. About a quarter of a second more per case. The golden dumps (10.3) run on every preset in `ctest`, so no marker text is needed in CI.

## 12. Invariants

For every world `load_history` and `MarkerIndex::load` accept and every Y in [0, E]:

1. The settlement entries at Y are exactly the settlements with `founded_year` ≤ Y (minus status 4 without `faded`), ascending id, with the fields of `settlement_state(s, Y)`; once present a settlement stays present (with `faded`).
2. At E the entries equal the `settlement` rows' end state (owner, kind, status, population).
3. A settlement's type is `ruin` from R(s) until the year its `ruin_faded` event starts, then `faded`; it is never `standing` again.
4. Every civilization alive at Y has exactly one capital entry, standing and owned by it; no entry of a dead civilization or of another owner is a capital; at E capital(c, E) = `civilization.capital` for every alive c.
5. R(s) = `ruined_year` for every settlement ruined at E; the ruin event of a faded settlement is dated R(s).
6. The union of the places' events is exactly the events of Y with a settlement or region; each event in one place; a place's symbol is the maximum of its events' symbols; places are in ascending smallest event id.
7. Passes are every `pass` row, independent of Y.
8. `markers_at` is a function of (world, Y, options): independent of earlier calls; the reusing form equals the returning one.
9. `draw_markers` at scale k equals the scale-1 result scaled by k; every pixel it changes is `MARKER_INK`, `MARKER_HALO`, `MARKER_FADED`, a civilization colour or a symbol colour.

## 13. Determinism notes (for the auditor)

The marker set and the overlay pixels are part of the cross-platform contract (CLI images, `markers.txt`, the golden file).

| Concern | Rule |
| --- | --- |
| Arithmetic | `int64`, `int32` grid offsets, no floating point in `markers.cpp`; predicates are integer squares and absolute values (radius ≤ 11) |
| Order | every list in the order section 4 states; places grouped by cell with the order of 3.4 (no unordered container; a map keyed by cell, if used, is ordered and its order not used for output); capital runs in event id order; binary search target "last entry with year ≤ Y" |
| Keys | kind keys and role names compared as bytes against the stored catalogue (V7) |
| Memory | `MarkerIndex` owns its copies; no reference into `LoadedHistory` or the reader is kept |
| Pixels | `draw_markers` writes only the colours of 6.2, overwriting in the order of 6.1; clipping by integer bounds |
| Errors | data problems are `load` values; FatalError only for a caller's year, id, buffer size or glyph key |

Lint: the new files are in scope; nothing banned is expected. Worth a look: the capital rule's role match (entity type 3 and the role name, not the seq); the `ruin_faded` fallback taking the smallest cause; the radius loops' bounds; that `history_report.cpp` output is unchanged after the word tables move.

## 14. What changes in recorded output

| Output | Change |
| --- | --- |
| World files, `p0-`, `p1-`, `p3-worlds.txt`, every existing golden file, `report`, `hash`, `render` without `--markers` | none; any difference is a defect |
| `render --help`, the `--year` usage message | P0-04 amendment 24 |
| `p4-04-markers.txt` | new (test writer) |
| `<preset>-<case>.markers.txt` | new file kind (build engineer; compared across presets) |

`GENERATOR_VERSION` stays 0.22.0: no world byte or generator golden vector changes.

## 15. Amendments to other specs (applied by the lead at merge unless done here)

- **P0-04 amendment 24** (applied with this spec, `docs/specs/p0-04-worldgen-cli.md`): section 7 of this spec.
- **P4-00 3.1, P4-04 paragraph and table row**: side `worldgen`, `worldgen-cli`, then `gdext`; markers are a library answer with a CLI overlay (section 1). Not edited here, to keep this branch clear of the P4-03a merge's edits to P4-00; the lead adds the pointer line.
- **P4-03 3.4**: the marker step between its steps 3 and 4 (8.1); 10.2 is unchanged (markers are outside every image and legend it checks). Pointer line likewise by the lead.
- **CLAUDE.md, README** (lead, at merge): `worldgen/inspect/markers.hpp`, the CTest entries `worldgen-tests-markers` and `worldgen-tests-slow-markers`, the golden file, `markers.txt` in the `world-hashes.sh` paragraph.

## 16. Open questions for Paul

Each with the recommendation and the interim choice the implementation takes.

1. **Size by kind, not by population.** The overview asked for size by population band. Kind is already the engine's population band (P3-05 8.2, with hysteresis), so a second, population-based size would mostly repeat it and need thresholds the world does not carry. Recommendation and interim: shape and size by kind (5.2), population in the hover text. Alternative: three sizes by population inside each shape.
2. **Event places as coloured rings, one per place.** Several events at one place in one year (a siege, then a sack) show one ring with the highest symbol (3.4), and the hover lists them all. Recommendation and interim: as specified. Alternative: one ring per event, nested, which clutters the busiest years.
3. **Markers without history.** Passes are geography, but the overlay needs the history (the index is built from it). Recommendation and interim: no markers on a world without history (a geography-only world shows passes in its `[geography]` report only). If Paul wants passes on P1 worlds, a later change splits the index.
4. **Hidden settlements.** Refuges that the history hides for `hidden_years` (P3-09) are drawn like any other: the viewer shows what happened, not what other realms knew. Recommendation and interim: shown. Alternative: an outline-only glyph while hidden, which needs the hidden span from the library.
5. **Markers on by default.** The walkthrough (P4-00 10 step 1) opens with markers on. Recommendation and interim: on, with faded ruins off.

## 17. Readings chosen

1. "Declining shown as an outline" (P4-00 3.1): the fill becomes a one-cell ring of the owner's colour with the map showing inside (5.2).
2. The capital at Y is derived from three event kinds (3.2) and checked against `civilization.capital` at E and against "every alive civilization has one" at every year (invariant 4).
3. A faded ruin's ruined year comes from the census and its ruin event from the cause of `ruin_faded` (3.3), since the row's columns are cleared when it fades.
4. A ring sits at the settlement's cell even when the settlement is a faded ruin that is not drawn; an event with only a region is at the region's seed cell (P4-00 3.1).
5. CLI glyphs are fixed in cells (they scale with `--scale`); viewer glyphs are fixed in screen pixels (g of 8.4). Both use the same glyph grids.
6. The key list is a sidebar list of its own, not rows of the view legend, so the legends that P2 to P4-03 check stay as they are.
