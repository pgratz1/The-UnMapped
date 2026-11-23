# P4-01 Timeline in the library

Status: **revision 2, 2026-11-22** (corrections after integration, section 15; revision 1 approved by the lead); **revision 3, 2026-11-22: region names by year for P5-12, generator 0.22.0, section 16**. Phase P4 Viewer v1, spec 1 (P4-00 3.1). Written from P4-00 (3.1, V3, V5, V6, 6.3, 7, 8, 9), P3-04 (6.6, 6.7, 9, 10, 12), P3-07 (5, 6.1, 12, 14), P0-04 (4, 6, amendments 14, 16, 18 (e), 21), P1-00 10.2, P1-04 9 and the code on `main` at fdd7b2f (`worldgen/include/worldgen/history/digest.hpp`, `event_catalogue.hpp`, `worldgen/include/worldgen/inspect/render.hpp`, `worldgen/src/inspect/history_report.cpp`, `tools/worldgen-cli/cli.cpp`). Numbers quoted for seed 42 are from the P4-00 worlds at generator 0.20.0 (`~/.cache/the-unmapped/out/p4-00-spec/`).

**No world byte changes.** Generator stays **0.20.0**, schema stays **7**, no stream, no stage, no data, no `meta` key. One new golden file (13.4) and a P0-04 amendment (23, applied with this spec).

## 1. Purpose

The viewer must answer "what was the world like in year Y" fast and the same way the history report does (P4-00 V3). This spec adds to `worldgen` a read-only **timeline**: an index, built once on open from the tables that `read_history_tables` and `HistoryView::load` already read and check, that answers at any year Y in [0, E] who holds each region, the state of each settlement, the relations in force, the civilizations alive, the phase and age, and the events of Y; and, independent of Y, the events of a civilization, region or settlement and the events an event caused. It fixes the **civilization colours** (P4-00 6.3) as integer arithmetic, and adds the year-dependent image **`territory`**, the `regions` composite with each held region in its holder's colour, rendered by `worldgen-cli render --composite territory --year Y`.

## 2. What this spec touches

| Area | Files |
| --- | --- |
| Timeline | new `worldgen/include/worldgen/history/timeline.hpp`, `worldgen/src/history/timeline.cpp` |
| Load helper | `history_reads_disagree` made public: declared in `worldgen/history/digest.hpp`, defined in `digest_tables.cpp` (moved from the anonymous namespace of `history_report.cpp`, text unchanged); `build_history_report` calls it (report bytes unchanged); `timeline.hpp` includes `digest.hpp` |
| Colours, territory | new `worldgen/include/worldgen/inspect/territory.hpp`, `worldgen/src/inspect/territory.cpp`; `render.cpp` may expose its composite steps internally (no change to any existing composite's pixels) |
| CLI | `tools/worldgen-cli/cli.cpp`: `--composite territory`, `--year`, usage text (P0-04 amendment 23) |
| Tests | `worldgen/tests/timeline_*.cpp` (SHARD `timeline` pattern `*timeline_*`, SLOW `slow-timeline`), `tools/worldgen-cli/tests/cli_territory_test.cpp`, fixtures `worldgen/tests/data/timeline/`, reference `worldgen/tests/reference/p4_timeline_reference.py`, golden `worldgen/tests/golden/p4-01-timeline.txt` |
| CI | `tools/ci/world-hashes.sh` (build engineer): territory images of the history cases, 13.6 |

Not touched: `gdext/` (P4-03 binds the timeline), the `Composite` enum and `ALL_COMPOSITES` (5.4), every world-writing path.

## 3. Inputs

The world file only, through `read_history_tables` (P3-07 5.1, 5.3) and `HistoryView::load` (P3-04 8.5). For `territory` also the layers `elevation`, `biome`, `region` (required) and `water`, `river`, `flow_dir` (optional) and table `biome`, exactly as the `regions` composite (P0-04 amendment 18 (e)). No data root is read.

What the timeline uses from each, and what it adds:

| Source | Used | The timeline adds |
| --- | --- | --- |
| `HistoryTables` | `end_year`, `regions`, `races`, `ages`, `phases`, `civilizations`, `settlements`, `census`, `region_control`, `relations` | per-region interval offsets; per-settlement census offsets; the holder change years; per-year lookups (section 4) |
| `HistoryView` | `events()`: `row.year`, `row.end_year`, `row.region`, `row.settlement`, participants, causes | events by start year; the multi-year events; events by civilization, region and settlement; the effects (inverse of `event_cause`) |
| `HistoryDigest` | nothing | nothing; the viewer calls `digest_history` itself for peaks, milestones, summary set (V3) |

The timeline restates no check. Everything it relies on is checked before it is built (6.1).

## 4. Definitions at year Y

E = `meta.history_end_year`. Y is an integer in **[0, E]**. The state at Y is the state at the end of year Y, after its events (P3-04 10). The simulation runs over [0, E − 1]: no event, interval end, relation end or census row is dated E (P3-04 W2, P3-07 5.3). **Y = E is the present**: its state is the end state of the run (equal to the state at E − 1), it lies in no phase, and it belongs to the last age (reading: the present is still the last race's age; P3-03 tiles [0, E) only).

| Query | Definition at Y |
| --- | --- |
| **holder** of region r | the `region_control` row of r with `from_year` ≤ Y and (`to_year` NULL or Y < `to_year`): `civ`, `since` = `from_year`, `event` = its `event`. No such row: free, `civ` 0, `event` 0, `since` = the largest `to_year` ≤ Y among r's rows, or 0 when r has none (free since the start). Half-open [from, to): a region lost in Y is not held at Y. At most one row matches (P3-07 5.3 "overlaps") |
| **holder change years** | the distinct values of `from_year` and of non-NULL `to_year` over all rows, ascending. Y is in the set iff holders(Y) ≠ holders(Y − 1), with holders(−1) all free (one bookkeeping writes a row only on a change of owner, P3-04 6.7 item 1, so an interval never ends and restarts with the same civ in one year) |
| **settlement state** of s | **exists** iff `founded_year` ≤ Y. If it exists: the census row of s with the largest `year` ≤ Y (one exists: the first row is at `founded_year`, P3-07 5.3); `owner`, `kind`, `status`, `population` from it, `since` = its `year`. Otherwise every field 0. Between census rows the last row holds (no interpolation). Census rows are written on a change of owner, kind or status and at phase ends (P3-04 6.7 items 3, 4), so **population at Y is the population last recorded at or before Y**, which within an active phase can lag the simulated value by up to the phase's length. Ruined (status 3) and faded (status 4) settlements exist; `owner` is the census value (0 when abandoned). At Y = E the state equals the `settlement` row's end state (P3-07 5.3) |
| **relations in force** | the `relation` rows with `since` ≤ Y and (`until` NULL or Y < `until`), as indexes into `HistoryTables::relations` (primary-key order), ascending. A row with `until` = `since` is in force in **no** year (P3-04 6.7 item 2); such a relation is visible only through its events and the row list. At Y = E exactly the rows with `until` NULL |
| **civilizations alive** | `founded_year` ≤ Y and (`ended_year` NULL or Y < `ended_year`), ascending id. A civilization ended in Y is not alive at Y |
| **phase** | the id of the `phase` row with `start_year` ≤ Y ≤ `end_year`; **0 at Y = E**. For Y < E a year no phase covers gives 0 (cannot happen on checked data) |
| **age** | the id of the `age` row with `start_year` ≤ Y ≤ `end_year`; at Y = E the id of the last `age` row (0 with no age rows). For Y < E a year no age covers gives 0 (cannot happen on checked data) |
| **events of Y** | the events with `year` ≤ Y ≤ `end_year`, ascending id (P4-00 2). Empty at Y = E. Events of a quiet step are dated at its first year (P3-04 6.6), so a quiet step's changes appear in that year |
| **events starting in Y** | the events with `year` = Y, ascending id (a subset of the events of Y; what a ticker prints once) |
| **previous / next event year** | the largest `year` of an event below Y / the smallest above Y, or none |

Independent of Y:

| Query | Definition |
| --- | --- |
| events of civilization c | events with a participant of entity type 3 and id c, ascending id, each once (its count is the digest's `CivilizationDigest::events`) |
| events of region r | events with `region` = r or a participant of type 5 and id r, ascending id, each once |
| events of settlement s | events with `settlement` = s or a participant of type 4 and id s, ascending id, each once |
| causes of e | `HistoryView::Event::causes` (ascending; not duplicated by the timeline) |
| effects of e | the events f with e among f's causes, ascending id |

## 5. Public API

### 5.1 Timeline (`worldgen/history/timeline.hpp`)

```cpp
namespace worldgen::history {

struct RegionHolding { std::int64_t region = 0, civ = 0 /*0 free*/, since = 0, event = 0 /*0 free*/; };
struct SettlementState {
    std::int64_t settlement = 0; bool exists = false;
    std::int64_t owner = 0, kind = 0, status = 0, population = 0, since = 0;   // all 0 when !exists
};
struct YearState {
    std::int64_t year = 0, phase = 0, age = 0;
    std::vector<RegionHolding> regions;        // one per HistoryTables::regions row, ascending id
    std::vector<SettlementState> settlements;  // one per settlement, index id - 1
    std::vector<std::size_t> relations;        // indexes into HistoryTables::relations / Timeline::relations(), ascending
    std::vector<std::int64_t> civilizations;   // alive at Y, ascending id
    std::vector<std::int64_t> events;          // year <= Y <= end_year, ascending id
    std::vector<std::int64_t> events_starting; // year == Y, ascending id
};

class Timeline {
public:
    // Preconditions: both from one world and checked (load_history, 5.2). Never throws on a
    // pair load_history accepted. Every Timeline FatalError text starts "timeline: ".
    [[nodiscard]] static Timeline build(const HistoryView& view, const HistoryTables& tables);

    [[nodiscard]] std::int64_t end_year() const noexcept;                       // E
    [[nodiscard]] bool valid_year(std::int64_t y) const noexcept;               // 0 <= y <= E
    [[nodiscard]] std::span<const std::int64_t> region_ids() const noexcept;    // HistoryTables::regions ids, ascending
    [[nodiscard]] std::size_t civilization_count() const noexcept;              // C
    [[nodiscard]] std::size_t settlement_count() const noexcept;                // S
    [[nodiscard]] std::size_t event_count() const noexcept;                     // number of events

    // At year Y (FatalError "timeline: year <Y> outside 0 to <E>" when !valid_year;
    // "timeline: no region <r>", "... no settlement <s>", "... no civilization <c>", "... no event <e>").
    [[nodiscard]] RegionHolding region_holder(std::int64_t region, std::int64_t year) const;
    [[nodiscard]] SettlementState settlement_state(std::int64_t settlement, std::int64_t year) const;
    [[nodiscard]] YearState year_state(std::int64_t year) const;
    void year_state(std::int64_t year, YearState& out) const;   // reuses out's buffers; same content
    [[nodiscard]] std::optional<std::int64_t> previous_event_year(std::int64_t year) const;
    [[nodiscard]] std::optional<std::int64_t> next_event_year(std::int64_t year) const;

    // Independent of Y. Spans into the timeline's own storage, valid while it lives.
    [[nodiscard]] std::span<const std::int64_t> holder_change_years() const noexcept;
    [[nodiscard]] std::span<const db::RelationRow> relations() const noexcept;   // = HistoryTables::relations
    [[nodiscard]] std::span<const std::int64_t> events_of_civilization(std::int64_t civ) const;
    [[nodiscard]] std::span<const std::int64_t> events_of_region(std::int64_t region) const;
    [[nodiscard]] std::span<const std::int64_t> events_of_settlement(std::int64_t settlement) const;
    [[nodiscard]] std::span<const std::int64_t> effects(std::int64_t event) const;
};

// Everything a reader of history needs, read once (P4-00 V3).
struct LoadedHistory { HistoryTables tables; HistoryView view; Timeline timeline; };
[[nodiscard]] db::Result<LoadedHistory> load_history(const db::WorldReader& world);

}

// In worldgen/history/digest.hpp (defined in digest_tables.cpp), moved from history_report.cpp
// (text unchanged): the two reads saw different files.
namespace worldgen::history {
[[nodiscard]] std::optional<std::string> history_reads_disagree(const HistoryView&, const HistoryTables&);
}
```

The timeline **owns copies** of what it indexes (it holds no reference into `HistoryTables` or `HistoryView`), so `LoadedHistory` may be moved. The engineer may add members; the semantics above are the contract. No exception for bad data: every data problem is an error value of `load_history`; a FatalError means the caller passed a year or id outside the world, like `render_event`'s (P3-04 8.5).

### 5.2 `load_history`, in order (first failure returned)

1. `read_history_tables(world)`: its errors unchanged (`history_missing` for a world without history, `corrupt_layer`, reader errors).
2. `HistoryView::load(world)`: its errors unchanged.
3. `history_reads_disagree`: `corrupt_layer`, `corrupt table 'event': the file changed while it was read`.
3a. What the timeline relies on in the event log, read separately from the tables: an event whose id is not the next in 1, 2, … or whose `year` is below the previous event's, or a participant, place (`region`, `settlement`) or cause referencing an id the tables lack: `corrupt_layer`, `corrupt table 'event': the file changed while it was read` (the same message; reached only when the file changed between the reads).
4. `Timeline::build`. Never throws on a pair that passed items 1 to 3a.

`std::bad_alloc` anywhere: `db::ErrorCode::out_of_memory`, `out of memory while building the timeline`. Items 1 to 3 are the order of `build_history_report` (P3-07 5.2 items 1 to 4a), so a world the report refuses is refused here with the same message; item 3a can refuse only a file that changed between the reads, with the report's message for that case.

### 5.3 Colours and territory (`worldgen/inspect/territory.hpp`)

```cpp
namespace worldgen::inspect {
inline constexpr std::uint64_t TAG_COLOUR = 0x524C4F433A4D4E55ULL;   // "UNM:COLR", P0-01 7.2 form
[[nodiscard]] Rgb race_base_colour(std::string_view race_key);          // 5.5
[[nodiscard]] Rgb civilization_shade(Rgb base, std::int64_t ordinal);   // 5.5; FatalError if ordinal < 0
// Index id - 1. FatalError "civilization_colours: civilization <c>: race <r> is not a race"
// when a civilization's race has no race row.
[[nodiscard]] std::vector<Rgb> civilization_colours(const history::HistoryTables&);

// Index region id; size = min(largest region id, 65535) + 1 (1 with no regions); [0] and free
// regions empty. Region ids need not be consecutive; ids below 1 or above 65535 are left out
// (the u16 layer region cannot hold them). FatalError: a year outside 0..E with the timeline's
// text ("timeline: year <Y> outside 0 to <E>"), also when the world has no regions; a held
// region whose civ c has no entry in colours: "territory_fill: no colour for civilization <c>".
[[nodiscard]] std::vector<std::optional<Rgb>> territory_fill(const history::Timeline&,
                                                             std::span<const Rgb> colours, std::int64_t year);

class TerritoryBase {   // the year-independent part, built once (5.6)
public:
    // FatalError when composite_regions_problem(...) reports a problem.
    [[nodiscard]] static TerritoryBase build(const db::LayerGrid& elevation, const db::LayerGrid* water,
                                             const db::LayerGrid* river, const db::LayerGrid* flow_dir,
                                             const db::LayerGrid& biome, const db::LayerGrid& region,
                                             std::span<const BiomeColour> biome_table);
    [[nodiscard]] std::uint32_t width() const noexcept; [[nodiscard]] std::uint32_t height() const noexcept;
    struct Cell {
        Rgb base;                  // B(x, y)
        std::uint16_t factor = 0;  // hillshade factor (160..352) when fillable, else 0
        std::uint16_t region = 0;  // the region value when fillable, else 0
    };
    [[nodiscard]] std::span<const Cell> cells() const noexcept;   // row-major, width x height
};
// 3 (width scale)(height scale); FatalError when scale is outside 1..16 or the size overflows.
[[nodiscard]] std::size_t territory_rgb_size(const TerritoryBase& base, std::uint32_t scale);
// The territory image (5.6), laid out as render_rgb. FatalError: scale outside 1..16, size overflow,
// out.size() != territory_rgb_size(base, scale).
[[nodiscard]] std::vector<std::uint8_t> render_territory(const TerritoryBase&, std::span<const std::optional<Rgb>> fill,
                                                         std::uint32_t scale);
void render_territory(const TerritoryBase&, std::span<const std::optional<Rgb>> fill, std::uint32_t scale,
                      std::span<std::uint8_t> out);
}
```

### 5.4 Why `territory` is not a `Composite` enumerator

The `Composite` enum and `ALL_COMPOSITES` are geography-only images that the viewer lists from the layers alone (`gdext/src/world_file.cpp` loops over `ALL_COMPOSITES`, and switches on the enum). `territory` needs the history and a year, so it gets its own functions; `--composite territory` is a CLI name only (P0-04 amendment 23), and P4-03 adds the view. This keeps P4-01 free of any `gdext` change. P4-02's legend catalogue covers the three geography composites; the territory legend (the civilizations alive at Y with their colours) is P4-03's, from `YearState::civilizations` and `civilization_colours`.

### 5.5 Civilization colours

A function of the stored world only: a civilization's colour depends on its race's **key** and its **ordinal** k = the number of civilizations with the same `race` and a smaller id. Nothing depends on race ids, on the number of civilizations or races, or on the year, so colours never change while Y moves, and civilizations or races added later (P5) never change the colour of an existing civilization of another race, or of a lower ordinal.

**Base colour** by race key (byte-exact comparison):

| Key | Family | Base (R, G, B) |
| --- | --- | --- |
| `elf` | green | (52, 150, 70) |
| `goblin` | yellow-brown | (190, 160, 50) |
| `dwarf` | red-brown | (160, 82, 45) |
| `faerie` | violet | (150, 90, 200) |
| `human` | blue | (50, 105, 200) |
| `orc` | red | (200, 40, 35) |
| any other key | from the key | h = `hash_words(TAG_COLOUR, encode_bytes(key))` (P0-01 7.1, 7.2); channel j (0 R, 1 G, 2 B) = 48 + floor(((h >> 8j) & 0xFF) × 160 / 255) |

**Shade** s = k mod 4, per channel c of the base (floor division on non-negative integers):

| s | Shade | Channel |
| --- | --- | --- |
| 0 | base | c |
| 1 | light | c + floor((255 − c) × 8 / 16) |
| 2 | dark | floor(c × 10 / 16) |
| 3 | deep | floor(c × 7 / 16) |

A fifth civilization of one race (k = 4) takes the base again, and so on (P4-00 6.3).

Worked table (every value checked by the reference, 13.2):

| Race key | s = 0 | s = 1 | s = 2 | s = 3 |
| --- | --- | --- | --- | --- |
| `elf` | (52, 150, 70) | (153, 202, 162) | (32, 93, 43) | (22, 65, 30) |
| `goblin` | (190, 160, 50) | (222, 207, 152) | (118, 100, 31) | (83, 70, 21) |
| `dwarf` | (160, 82, 45) | (207, 168, 150) | (100, 51, 28) | (70, 35, 19) |
| `faerie` | (150, 90, 200) | (202, 172, 227) | (93, 56, 125) | (65, 39, 87) |
| `human` | (50, 105, 200) | (152, 180, 227) | (31, 65, 125) | (21, 45, 87) |
| `orc` | (200, 40, 35) | (227, 147, 145) | (125, 25, 21) | (87, 17, 15) |
| `troll` (unknown) | h = 0x90674847EA21F9EA: (194, 204, 68) | | | |
| `` (empty key) | h = 0x665E00601317AE04: (50, 157, 62) | | | |

Seed 42, 1024: civilizations 1 and 2 (elves) are elf s0 and s1; 3, 7, 10, 12 (goblins) goblin s0 to s3; 4, 5 dwarf s0, s1; 6 faerie s0; 8, 9 human s0, s1; 11 orc s0.

### 5.6 The `territory` image, pixel by pixel

Built on the `regions` composite (P0-04 amendment 18 (e)) at scale 1, then scaled. For cell (x, y), with e = `elevation`(x, y), v = `region`(x, y) and B(x, y) the `regions` composite's pixel:

1. **Fillable.** The cell is fillable iff all of: v > 0; e ≥ 0 (step 2 of the composite map coloured it: shaded land, not ocean); it is not drawn by step 3 (`water` given and `water`(x, y) = 2); it is not drawn by step 4 (with `water` given: without `river`, `water`(x, y) ≠ 1; with `river`, no entry of the paint list was painted on the cell, whatever its colour; so a cell with `river` class 0 and `water` 1 that no entry paints is fillable); it is not a border cell (the black rule of amendment 18 (e)).
2. **Pixel.** If the cell is fillable, v < `fill.size()` and `fill[v]` holds colour C: `shade_colour(C, hillshade_factor(elevation, W, H, x, y))` (P1-00 10.2 step 2, the same factor the biome colour received). Otherwise B(x, y).
3. **Scale.** At `--scale k` each cell's colour fills its k × k block, as every composite.

So: ocean, lakes, rivers (including the river widening painted on dry neighbours) and region borders are exactly as in `regions`; free land is the `regions` composite unchanged (P4-00 6.3); held land is the holder's colour with the relief shading of the biome it replaces. `territory_fill(t, colours, Y)` sets `fill[r]` = `colours[c − 1]` for each region r held by c at Y. A `region` value with no region row, or ≥ `fill.size()`, is unfilled (with `territory_fill`, a region id above 65535 never has an entry).

`TerritoryBase` holds, per cell at scale 1, B, the factor, and v when fillable (else 0); `render_territory` is then one lookup per cell, so P4-03 rebuilds the image from a new fill without touching the layers (P4-00 8). Equalities that pin it down: `render_territory(base, {}, k)` = `render_composite_regions(…, k, biome_table)` byte for byte; a fill with every region empty gives the same.

Example (4 × 1, no water, river or flow_dir; elevation 10 everywhere, so every factor is 256; `region` `1 1 2 0`; region 1 held by an elf s0, region 2 free): cell 0 is fillable, pixel (52, 150, 70); cell 1 is a border cell (E neighbour 2), black; cell 2 is free: B; cell 3 has v = 0, so it is never fillable: B.

## 6. Algorithm and index

### 6.1 Build (one pass over each table; rows arrive in primary-key order)

| Index | Built from | Size |
| --- | --- | --- |
| region positions | `tables.regions` (ascending id); id → position by binary search | R |
| control offsets | `region_control` is ordered by (region, from_year): offsets[pos] … offsets[pos+1] | R + 1 |
| census offsets | `census` ordered by (settlement, year); settlement ids are 1 … S (P3-07 5.3) | S + 1 |
| holder change years | sort and deduplicate `from_year` and non-NULL `to_year` | ≤ 2 × rows |
| event start years | `event.year` is non-decreasing with id (P3-04 11.1), so the ids of year Y are a contiguous run, found by binary search | — |
| multi-year events | ids with `end_year` > `year`, ascending | 9 at seed 42 |
| per-entity event lists | one pass over events in id order and their participants in seq order; an id is appended to a list only if it differs from the list's last element (so each event once, ascending) | Σ participants |
| effects | for each event f in id order, for each cause e: append f to e's list | `event_cause` rows |

Cost O(n log n) at most (the sort of change years); everything else O(n). Memory at 1024: under 1 MB.

### 6.2 Answers

| Query | Method | Cost |
| --- | --- | --- |
| `region_holder` | binary search on `from_year` in the region's run (last row with `from_year` ≤ Y), then the `to_year` test; when the region is free its `since` is that row's `to_year` (rows do not overlap, so earlier rows end earlier), or 0 when no row has `from_year` ≤ Y | O(log k_r) |
| `settlement_state` | binary search on `year` in the settlement's run | O(log k_s) |
| relations in force | linear scan of `relations` | O(rows), 77 at seed 42 |
| civilizations alive | linear scan | O(C) |
| phase, age | binary search on `start_year` | O(log) |
| events of Y | the ids with `year` = Y (binary search for the run) merged with the multi-year events with `year` < Y ≤ `end_year` (scan), ascending | O(log n + k + m) |
| previous / next event year | binary search on the start years | O(log n) |

`year_state` at 1024 is a few thousand integer operations, far below P4-00 8's 2 ms (budget 8 ms at 2048). `render_territory` at 1024 is 1,048,576 lookups and at most as many `shade_colour` calls, within the 40 ms budget; `TerritoryBase::build` costs one `regions` composite and is done once per open.

Measured at integration (`linux-gcc-release`, seed 42 at 1024, bench `timeline-bench`): worst single `year_state` call 0.34 ms against the 2 ms budget; `render_territory` 3.9 ms against 40 ms; `TerritoryBase::build` 46 ms, once per open.

## 7. Command line (P0-04 amendment 23)

```
worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME [--year Y]) [--out PATH]
                    [--scale N] [--force] [--verbose]
```

- `--composite territory` renders section 5.6 at year Y. `--year Y` is a decimal integer (P0-04 4.2); default **E** (the present, as the viewer opens, P4-00 V6).
- Default output `<build root>/out/<db file stem>.territory-<Y>.png` (Y in decimal, always present, also when defaulted). On success stdout is the absolute path, as every `render`.
- **Order of checks**, first failure reported:
  1. Usage (exit 2), in this order: the existing checks (exactly one of `--layer` and `--composite`, the composite name, `--palette` with `--composite`, the palette name, `--scale`); then `--year` without `--composite territory`: `--year applies to --composite territory only`; then the value is parsed: a malformed value gives `invalid --year '<text>'; expected a decimal integer without sign or leading zeros` (the shared integer parser's message). The parser reads into `uint64`, so a value of 2^64 or more (for example `18446744073709551616`) gives this malformed-value message, not the range message; an empty value (`--year ""`) gives it too, with `''` as the text. A `--year` given as the last argument with no value is the parser's `option '--year' needs a value`.
  2. Open the world (exit 3, unchanged).
  3. Geography: exactly the `regions` composite's checks and messages, with `territory` as the composite's name (`cannot render the composite territory: …; available layers: …`; missing `elevation`, `biome` or `region` as the reader's missing-layer error), exit 3.
  4. History: `load_history`. A world without history (`history_missing`): `worldgen-cli: error: world file '<path>' has no history (stages: <stages or none>); generate it with the history stage`, exit 3, the message of `report --history`. Corrupt history: its message, exit 3. Out of memory: exit 1.
  5. Year range: a parsed Y > E (every value from 2^63 to 2^64 − 1 included): `--year <text> is outside the history of world file '<path>' (0 to <E>)`, with `<path>` the world file's path as opened, **exit 2** (an option value, P0-04 4.6; it can be judged only after opening the file). Like every usage error it is followed by the usage hint. Nothing is written.
  6. The image size limit (P0-04 amendment 7), the output location (exit 4), then render and write, as every `render`.
- `render --help` (lines replaced or added, exact):

```
usage: worldgen-cli render --db PATH (--layer NAME [--palette P] | --composite NAME [--year Y]) [--out PATH]
                           [--scale N] [--force] [--verbose]
...
                      regions: the biomes composite with the borders of layer region in black;
                      territory: the regions composite with each region held in the year in the
                      colour of its holder (needs the history)
  --year Y            with --composite territory: the year, 0 to the history's end year
                      (default the end year, the present)
  --out PATH          output PNG (default <build root>/out/<db file stem>.<layer>.png,
                      <db file stem>.<composite>.png for --composite, or
                      <db file stem>.territory-<year>.png for territory)
```

  and `invalid --composite '<x>'; expected map, biomes, regions, territory`.

## 8. Invariants

For every world that `load_history` accepts and every Y in [0, E]:

1. `region_holder(r, Y)` equals a linear scan of r's `region_control` rows with the definition of section 4; at most one row matches.
2. Y is in `holder_change_years()` iff the holder vector at Y differs from that at Y − 1 (all free at −1); the holders at E are exactly the rows with `to_year` NULL; their count is `[stats]` `regions_held`.
3. `settlement_state(s, E)` equals the `settlement` row's owner, kind, status and population; a settlement exists at Y iff `founded_year` ≤ Y, and once it exists it exists at every later year.
4. `year_state(E)` and `year_state(E − 1)` have equal `regions`, `settlements`, `relations` and `civilizations`; at E, `phase` = 0, `age` = the last age, `events` and `events_starting` are empty.
5. Each event e appears in `events` of exactly the years `year` … `end_year` and in `events_starting` of `year` only; the `events` of Y are ascending.
6. `events_of_civilization(c).size()` = `digest_history(...).civilizations[c − 1].events`; every list is strictly ascending; e ∈ `effects(c)` iff c ∈ causes of e.
7. `relations` of Y never contains a row with `until` = `since`; at E it is exactly the rows with `until` NULL; `civilizations` at E equals the rows with `ended_year` NULL.
8. Colours: a function of (race key, ordinal) only; civilizations of one race with ordinals below 4 have distinct colours (true for the six keys and the two unknown examples of 5.5; the reference checks all four shades distinct for each).
9. `render_territory(base, {}, k)` = `render_composite_regions(…, k)`; every pixel of `render_territory` is either B or, on a fillable cell of a held region, `shade_colour(colour of its holder, factor)`.
10. Nothing depends on the order of calls: `year_state(Y)` is the same whatever years were asked before, and the reusing overload gives the same content as the returning one.

## 9. Determinism notes (for the auditor)

The timeline's answers and the territory image are part of the cross-platform contract (the viewer and the CLI compare them pixel for pixel, P4-00 V4, 7).

| Concern | Rule |
| --- | --- |
| Arithmetic | `int64` and `uint64` only; shades and channel maps are floor divisions of non-negative values; `hash_words` as P0-01; no floating point in `timeline.cpp` or `territory.cpp` |
| Order | every list in ascending id or year; binary searches with stated targets (last row with key ≤ Y); per-entity lists built in event id order; no unordered container; no pointer order; change years sorted with `std::sort` on integers then deduplicated (total order, so the result is unique) |
| Keys | race keys compared as bytes; no locale, no case folding |
| Memory | the timeline owns its copies; spans returned are into its storage; no reference into `HistoryTables` or `HistoryView` is kept (a moved `LoadedHistory` stays valid) |
| Pixels | `render_territory` uses `shade_colour` and `hillshade_factor` of `render.hpp`; no existing composite's pixels change (their tests stay as they are) |
| Errors | data problems are values (5.2); FatalError only for a caller's year or id; `bad_alloc` caught in `load_history` |
| Time | no clock; the bench (13.5) is a skipped suite and prints only |

Lint: the new files are in scope; nothing banned is expected. What the auditor should look at in particular: the free-region `since` rule and the binary search bounds at Y = 0 and Y = E; the multi-year merge keeping ascending order; that "fillable" in `TerritoryBase` is taken from the composite's own step-3/step-4 drawing (a painted flag), not re-derived from colours; integer widths in the TAG_COLOUR channel map; that `history_reads_disagree` moved with its text unchanged.

## 10. Readings chosen where the overview is silent

1. **Y = E**: the end state, no phase (0), the last age, no events (section 4).
2. **Half-open** intervals for holders, relations and alive civilizations (the state at the end of Y): a region lost, a relation closed or a civilization ended in Y is gone at Y. A relation with `until` = `since` is in force in no year (P3-04 6.7).
3. **Population between census rows** is the last recorded value (no interpolation); stated in section 4 so that the viewer does not present it as exact.
4. **Region and settlement event lists** include both the event's place and participants of that type.
5. **Held land is filled, not tinted**: the holder's colour with the relief shading, so a region's colour matches its legend entry whatever the biome; water, borders and free land stay as in `regions`. No separate realm border is drawn (the fill shows it).
6. **`--year` defaults to E**, and an out-of-range year is a usage error (exit 2).
7. **Unknown race keys** get a colour from the key's hash (stable across worlds), so a P5 race or a test fixture's race renders without a code change.

## 11. Proposals

None. The overview's proposals entry "Viewer v1: the library answers the year, the viewer draws it" covers this spec; the colours are P4-00 question 4.

## 12. Amendments

- **P0-04 amendment 23** (applied with this spec): `render --composite territory`, `--year`, default name, checks and help text of section 7.
- **P3-07**: none in its text; `reads_disagree` becomes the public `history_reads_disagree` with the same message (an implementation move).
- **CLAUDE.md, README** (lead, at merge): the new files, the CTest entries `worldgen-tests-timeline`, `worldgen-tests-slow-timeline`, `worldgen-cli-tests` group for `cli_territory_test.cpp`, the golden file.

## 13. Tests required

The test writer works from this spec alone. Reference: `worldgen/tests/reference/p4_timeline_reference.py`, Python 3, exact integers, reading worlds with `sqlite3` only (layers decoded from their blobs per P0-02); it may reuse the core reference's `hash_words` and SHA-256 and the P3-07 reference's fixture builder; it does not port the C++.

### 13.1 Canonical dumps (used by the golden and property tests)

LF lines, single spaces, decimal integers, `-` for an empty list.

**Year dump** D(Y), in this order:

```
year <Y> phase <phase> age <age>
hold <region> <civ> <since> <event>                                       one per region row, ascending id
settlement <id> <exists 0|1> <owner> <kind> <status> <population> <since>   one per settlement, ascending id
relation <index>                                                          in force, ascending
alive <civ>                                                               ascending
event <id>                                                                events of Y, ascending
start <id>                                                                events starting in Y, ascending
```

**Static dump** S:

```
end <E>
change <year>                         one per holder change year, ascending
colour <civ> <r> <g> <b>              one per civilization, ascending id
civ <id> <event ids or ->             one per civilization
region <id> <event ids or ->          one per region row
settlement <id> <event ids or ->      one per settlement
effects <event> <event ids or ->      one per event
```

### 13.2 Unit tests (C++, `worldgen-tests-timeline`)

1. `race_base_colour` and `civilization_shade`: the whole table of 5.5, `troll` and the empty key; k = 4, 5 wrap to s = 0, 1.
2. `civilization_colours` on a fixture with two races interleaved by id (ordinals counted per race, not per id).
3. On fixture T1 (13.3), each row of section 4's tables at the years that decide it, with expected values written in the test: a region held, lost and retaken (Y = from − 1, from, to − 1, to); a region never held (`since` 0) and one freed (`since` = its last `to_year`); a census gap (population held between rows); ruined and faded settlements; a settlement not yet founded; a relation with `until` = `since` absent from every year; an open relation at E; a civilization ending in Y absent at Y; Y = 0, E − 1, E; events of a multi-year event in the middle of its span and at both ends; `previous_event_year(0)`, `next_event_year(E)` none.
4. `effects` and the three entity lists on an event with two causes and a participant repeated across roles (listed once).
5. FatalError texts for a year −1 and E + 1, an unknown region, settlement, civilization and event (each starting `timeline: `); `civilization_shade` with ordinal −1; `civilization_colours` with a civilization whose race has no row; `territory_fill` with too short a colour list, and with a bad year (also on a world with no regions).
6. `load_history`: `history_missing` on `--until chronology`, a P3-07 5.3 corruption (same message as `report --history`); the disagreement message, by calling `history_reads_disagree` directly with the `HistoryView` of one world and the `HistoryTables` of another (there is no report test of 4a to copy).
7. Territory on T1's hand-made layers (at most 8 × 6 cells), expected pixels written in the test: a held cell, a border cell, a lake cell (step 3), a river cell and a dry cell painted by the river widening (step 4, unfilled), an ocean cell, a free cell, scale 2; and the equality `render_territory(base, {}, k)` = `render_composite_regions` at k = 1, 3.

### 13.3 Fixtures

- The P3-07 fixtures `worldgen/tests/data/history-report/` (`h1-war-and-ruin` … `h7-runs`): dumps only (they have no geography layers).
- New **T1 timeline edges** in `worldgen/tests/data/timeline/`, built twice as P3-07 14.3 (the C++ through the writer API, the reference through SQL; table hashes compared first): the cases of 13.2 item 3 and item 7's small layers (`elevation`, `water`, `river`, `flow_dir`, `biome`, `region`) and table `biome`.

### 13.4 Golden values

`worldgen/tests/golden/p4-01-timeline.txt`, written by `python3 -B worldgen/tests/reference/p4_timeline_reference.py golden`, LF, header lines starting `# ` (generator version 0.20.0, provenance, line forms). Cases in ascending byte order of their names: the world cases with history, which are the seven `geo-*` and `hist-*` cases of `world-hashes.sh` plus `sample-42-1024` (P3-07's `civs4-*` and `seed-0-1024` are not included), and the fixtures. Per case, in this order:

1. `<case> static <sha256>`: SHA-256 of S.
2. `<case> year <Y> <sha256>`: SHA-256 of D(Y), for each Y of {0, E − 1, E, floor(E / 2)} ∪ the age start years ∪ the holder change years, ascending.
3. `<case> years <sha256>`: SHA-256 of D(0) D(1) … D(E) concatenated (every year).
4. World cases only: `<case> territory <Y> <sha256>`: SHA-256 of the RGB buffer at scale 1 for each Y of the set {the smallest holder change year, floor(E / 2), E}, ascending, each year once (the smallest holder change year is omitted when no region ever changed hands) (the reference renders the `regions` composite itself from P1-00 10.2, P1-04 9 and amendment 18 (e), and then 5.6).
5. Fixtures: `<case> table <name> <hex>` as P3-07 14.4.

The 1024 cases run in the `*-slow` suite (`worldgen-tests-slow-timeline`). Re-recorded by the reference whenever a merge changes those worlds; never to make a C++ test pass.

### 13.5 Property and bench

8. Over 512 seeds 0–3 (default) and, in the slow suite, 512 seeds 4–19 and 1024 seeds 0–9: invariants 1–10 by brute force against the tables (linear scans per Y for every Y), the digest's counts, and `[stats]`.
9. Bench, skipped suite `timeline-bench` (as `engine-bench`): `load_history`, `year_state` for every Y and `render_territory` at 1024 seed 42; prints times only. The validation note records them against P4-00 8.

### 13.6 CLI tests (`cli_territory_test.cpp`) and cross-compiler

10. `render --composite territory` on a default 512 world: exit 0, the default name `<stem>.territory-<E>.png` printed; `--year 0`, `--year E`, `--scale 2`; `--out`, existing without `--force` exit 4.
11. Usage (exit 2): `--year` with `--layer`, with `--composite regions`; `--year 01`, `--year -1`, `--year x`; `--year E+1` and `--year 18446744073709551615` with the range message; `render --help` contains the lines of section 7 exactly. Not yet tested (to add): `--year 18446744073709551616` (2^64) and `--year ""` (empty), each with the malformed-value message of section 7 item 1.
12. Exit 3: `--test-pattern` world (missing layer message), `--until regions` and `--until chronology` worlds (the no-history message), a corrupted history table (the report's message).
13. The PNG's pixels (decoded by the test) equal `render_territory` of the library on the same world (the CLI formats nothing itself).
14. **Cross-compiler (recommended, build engineer):** `world-hashes.sh` renders, for each case with history, `territory` at Y = floor(E / 2) and Y = E and saves `<preset>-<case>.territory.txt` with the SHA-256 of each PNG (`territory <Y> <sha256>`); `compare` includes the file. The PNG writer is vendored and integer, so its bytes are a fair cross-compiler check; a timeline text is not needed in CI, because the golden dumps run on every preset in `ctest`.

## 14. Open questions

1. **Colours** (P4-00 question 4): the six bases and the four shades of 5.5 are interim; dwarf and orc darks are close. Paul or the world critic may change the table at P4-03, a change of golden values only (`colour` lines, territory hashes), no world byte.
2. **Realm borders**: none drawn (10, item 5). If the critic finds neighbouring realms of one race hard to separate, a border between differing holders is a small addition to 5.6.
3. **Population between census rows** (10, item 3): exact census only. If the viewer needs a finer population, P5 would have to record it; no interpolation is recommended.

## 15. Revision notes

**Revision 2** (2026-11-22, after integration on `p4/01-int`; approved by the lead; the implementation and tests already agree). No behaviour change; the spec now states what was built.

1. `history_reads_disagree` is declared in `worldgen/history/digest.hpp` and defined in `digest_tables.cpp`; `timeline.hpp` includes `digest.hpp` (2, 5.1).
2. `load_history` item 3a: an event log out of order or referencing an id the tables lack gives `corrupt_layer` with the "file changed" message, so `Timeline::build` never throws on an accepted file; `Timeline` FatalError texts start `timeline: ` (5.1, 5.2).
3. `territory_fill` size min(largest region id, 65535) + 1; ids below 1 or above 65535 left out; ids need not be consecutive (5.3, 5.6).
4. Age at E is the last row's; for Y < E an uncovered year gives phase or age 0 (4).
5. FatalErrors of `civilization_shade`, `civilization_colours` and `territory_fill` (5.3; tests 13.2 item 5).
6. A cell of river class 0 with water 1 that nothing paints is fillable (5.6 item 1).
7. CLI check order for `--year`, the range message's path and usage hint, values of 2^64 or more and an empty value (7; tests 13.6 item 11).
8. Accessors `Timeline::region_ids()`, `civilization_count()`, `settlement_count()`, `event_count()`, `territory_rgb_size()`, `TerritoryBase::cells()` (5.1, 5.3).
9. Property seeds: 512 seeds 0–3 by default; 512 seeds 4–19 and 1024 seeds 0–9 in the slow suite (13.5 item 8).
10. Territory golden years are a set, ascending, each once; the smallest holder change year omitted when no region changed hands (13.4 item 4).
11. Golden world cases: the seven `geo-*` and `hist-*` cases plus `sample-42-1024` (13.4).
12. The disagreement test calls `history_reads_disagree` directly (13.2 item 6).
13. Timings measured at integration (6.2).

## 16. Amendment r3: region names by year (P5-12, generator 0.22.0)

Normative from generator 0.22.0 (`docs/specs/p5-12-renamed-regions.md` 7.3; table `region_name`, schema 8, P3-04 r21 23.4). History renames regions; `region.name` stays the geographic name.

1. **Definition** (added to section 4): the **name** of region r at year Y is the `name` of r's `region_name` row with the largest `from_year` ≤ Y, or `region.name` when there is none. Half-open as every state at Y (the state at the end of Y): a region renamed in Y bears its new name at Y.
2. **API** (added to 5.1): `[[nodiscard]] std::string_view Timeline::region_name(std::int64_t region, std::int64_t year) const;` with the fatal texts of 5.1 for an unknown region or a year outside 0..E. `load_history` (5.2) takes the rows from `read_history_tables` (P3-07 r21 26.6), so its checks and errors are those; the index keeps per region its rows (offsets into one vector in primary-key order, as `region_control`'s), and the answer is a binary search on `from_year`, O(log k_r).
3. **Invariant** (added to 8): `region_name(r, E)` is the report's present name of r (P3-07 r21 26.4); `region_name(r, Y)` changes only at the `from_year` of r's rows.
4. **Dumps** (13.1): the static dump S gains, after the `region` lines, one line `name <region> <from_year> <name>` per `region_name` row in primary-key order; the year dump is unchanged. `p4-01-timeline.txt` is re-recorded by the test writer's reference (every world case changes with 0.22.0 anyway).
5. **Viewer** (for the viewer engineer, after this merge): the hover text and the region list name a region by `region_name(r, Y)` at the year the viewer shows (E when it opens, P4-00 decision 3), and may add the geographic name (`region.name`, from P4-02's `region_name()`) when the two differ. Until that change the viewer shows `region.name`; `gdext-headless-check` compares layers and composites only, so it is unaffected.
