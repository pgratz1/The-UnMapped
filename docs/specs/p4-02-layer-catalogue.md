# P4-02 Layer catalogue and region names in the library

Status: **draft, 2026-11-22**, for approval by the lead. Phase P4 Viewer v1, stage 2 of the overview `docs/specs/p4-00-viewer-v1-overview.md` (3.1 "P4-02", rules V1 to V10, 6.1, 7, 9). Design doc sections 6 (world database), 10 (tech stack, `worldgen` public API, world viewer) and 12 (open question "Layer catalogue in the library", answered by P4-00 6.1 and approved by the lead with that overview). Proposals read: "Viewer: layer units and region names come from a copy of the catalogue" (2026-11-18), "Viewer v1: the library answers the year, the viewer draws it" (2026-11-22). Depends on P1-00 section 4 (the layer catalogue), P1-00 10.2, P1-03 9, P1-04 9, P1-05 9 and P1-06 9 (palettes and composites), P1-06 3.2 (table `region`), P0-04 amendment 18 (e) (composite `regions`) and P2 (`gdext/README.md`).

**Generator stays 0.20.0, schema 7.** No world byte, no golden world hash, no `report` line, no `hash` line and no `render` pixel changes. No `GENERATOR_VERSION` bump.

## 1. Purpose

The viewer keeps its own copy of the layer catalogue of P1-00 section 4 (`gdext/src/viewer/layer_catalogue.cpp`: each layer's group, unit and display form, and the legend of every palette and composite) and reads the region name from a raw column of table `region`. This stage moves both into the `worldgen` public interface, so that the viewer, `worldgen-cli render` (through `registered_palette`) and, later, the create-world screen and the in-game map read one definition. The viewer then deletes its copy. Nothing visible changes: the headless check passes with the same assertions and the same messages.

## 2. Inputs

None from a world beyond what the functions are handed: layer names, grids (`db::LayerGrid`), `db::LayerInfo` lists, rows of tables `biome` and `region` as `read_biome_rows` and `read_region_rows` already return them. No seed stream, no parameter, no TOML.

## 3. Where it lives: code, not data

**Decision (interim, recommended):** the catalogue is C++ code, a `constexpr` table in the library, not a TOML data kind.

- Hard rule 4 ("data in TOML") covers game content that a designer tunes: races, beasts, cultures, biomes, event rules. The layer catalogue is not tunable content; it describes the generator's own output, which the stages' code fixes (P1-00 4 rule 3: changing a fixed layer's name, type, units or meaning is a schema change). A TOML copy could disagree with the code that writes the layer, and changing it alone would mean nothing.
- The palettes and ramp stops it builds legends from are already code (`worldgen/inspect/render.hpp`).
- The viewer would otherwise need the data root at run time just to label a layer, and every reader would need the loader.
- Where a layer's labels do depend on data (biome names and colours, region names), they come from the world's own tables, as now.

Recorded as a proposals entry ("The layer catalogue is code, not TOML data", appendix A) because it is a reading of a hard rule.

## 4. Public API

New header `worldgen/include/worldgen/inspect/layer_catalogue.hpp`, source `worldgen/src/inspect/layer_catalogue.cpp`, namespace `worldgen::inspect`. Plain C++20, no Godot (hard rule 2). Names below are binding for the engineer and the test writer; the engineer may add private helpers.

### 4.1 Groups

```
enum class LayerGroup : std::uint8_t { composites = 0, terrain = 1, water = 2, climate = 3,
                                       biomes = 4, resources = 5, regions = 6, other = 7 };
inline constexpr std::size_t LAYER_GROUP_COUNT = 8;
std::string_view layer_group_title(LayerGroup) noexcept;
```

Titles: `Composites`, `Terrain`, `Water`, `Climate`, `Biomes`, `Resources`, `Regions`, `Other` (list order = enumerator order). An out-of-range enumerator value gives `Other`.

### 4.2 Catalogue entries

```
struct SpecialValue { std::int64_t value; std::string_view label; };

struct LayerSpec {
    std::string_view name;
    LayerGroup group;
    db::ElementType type;            // the catalogue type of P1-00 4
    std::int64_t min;                // value range of P1-00 4
    std::int64_t max;
    bool max_is_cell_count;          // true: the real maximum is width * height (flow_accum)
    std::int64_t scale;              // decimal divisor to display units: 1 or 10
    std::string_view display_unit;   // "m", "°C", "mm/yr", "/ 255", or ""
    std::string_view unit;           // the unit and meaning in a few words (the viewer's legend title)
    std::string_view description;    // one sentence, from P1-00 4
    std::span<const SpecialValue> special;  // ascending value
    Palette palette;                 // the registered palette (--palette auto)
};

std::span<const LayerSpec> layer_catalogue() noexcept;       // the 19 entries, in layer id order
const LayerSpec* find_layer_spec(std::string_view name) noexcept;  // nullptr when unknown
LayerGroup layer_group(std::string_view name) noexcept;       // Other when unknown
std::string_view layer_unit(std::string_view name) noexcept;  // empty when unknown
```

The table, in this order (layer ids 1 to 19 of P1-00 3). `unit` strings are the viewer's today, byte for byte (they are its legend titles, so they must not change); `°` is written `°` in the source.

| name | group | type | min | max | scale | display_unit | unit | special | palette |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `elevation` | terrain | i16 | -30000 | 30000 | 10 | m | decimetres above sea level | — | hypsometric |
| `water` | water | u8 | 0 | 3 | 1 | | category: 0 dry land, 1 river, 2 lake, 3 ocean | 0 dry land, 1 river, 2 lake, 3 ocean | water |
| `water_depth` | water | u16 | 0 | 32768 | 10 | m | decimetres of standing water | 0 no standing water | gray |
| `flow_dir` | water | u8 | 0 | 8 | 1 | | D8 code of the downstream neighbour | 8 none (ocean) | direction |
| `flow_accum` | water | u32 | 0 | 4294967295, `max_is_cell_count` | 1 | | cells draining through the cell (x 256 m2) | 0 ocean | log2 |
| `river` | water | u8 | 0 | 15 | 1 | | river size class, 0 none | 0 not a river | river |
| `lake` | water | u32 | 0 | 4294967295 | 1 | | lake id, 0 none | 0 none | ids |
| `coast_dist` | water | i16 | -32767 | 32767 | 1 | m | metres to the coast, < 0 at sea | — | signed |
| `temperature` | climate | i16 | -600 | 500 | 10 | °C | annual mean, tenths of a degree Celsius | — | temperature |
| `rainfall` | climate | u16 | 0 | 10000 | 1 | mm/yr | annual precipitation, millimetres | — | rainfall |
| `biome` | biomes | u8 | 1 | 254 | 1 | | biome code of table biome | — | biome |
| `fertility`, `ore`, `timber`, `fresh_water`, `forage`, `habitability` (six rows, this order) | resources | u8 | 0 | 255 | 1 | / 255 | score 0 to 255 | 0 none | score |
| `walk_area` | resources | u16 | 0 | 65535 | 1 | | walkable area id, 0 none | 0 none | ids |
| `region` | regions | u16 | 0 | 65535 | 1 | | region id of table region, 0 ocean | 0 none (ocean) | ids |

`water_depth`'s 32768 is P1-02 H6's bound for any `i16` elevation (the elevation stage keeps it at 30000). `description` is one sentence per layer, taken from the "units and scale" and "special values" columns of P1-00 4 (for example `elevation`: "Height of the ground above sea level in decimetres; below 0 is ocean, above 0 is land, never 0."); the engineer writes them, ASCII plus `°`, at most 160 bytes each; they are not shown by the viewer in this stage and no test fixes their wording beyond non-empty and the byte limit.

**One source for the palette.** `registered_palette(name)` (render.hpp) becomes `find_layer_spec(name) ? spec->palette : Palette::gray`. Its results are unchanged for every name; the comment in render.hpp points to the catalogue. `auto_palette` and `palette_suits` are unchanged.

### 4.3 Values in display units

```
bool cell_value(const db::LayerGrid&, std::uint32_t x, std::uint32_t y, std::int64_t& value) noexcept;
std::string format_layer_value(std::string_view name, std::int64_t v);
```

`cell_value`: the cell as int64; false outside the grid, for an index past the value vector, and for a u64 value above INT64_MAX (the caller shows the raw decimal instead). Moved from the viewer unchanged.

`format_layer_value`, exactly the viewer's current strings. Notation: `T(v)` = v / 10 with one decimal, sign first, from the magnitude (`-5` → `-0.5`, `0` → `0.0`, INT64_MIN exact); `H(u)` = u / 100 with two decimals; `A(n)` = `H((256 n + 5000) / 10000)` (km², round half up); decimals by `db::format_i64` / `db::format_u64`; `|v|` the magnitude as u64.

| layer | v | text |
| --- | --- | --- |
| `elevation` | any | `T(v) m`, plus ` (sea)` when v < 0 |
| `water` | 0, 1, 2, 3 / other | `dry land`, `river`, `lake`, `ocean` / `category <v>` |
| `water_depth` | 0 / other | `0 m (no standing water)` / `T(v) m deep` |
| `flow_dir` | 0..7 / 8 / other | `E SE S SW W NW N NE` by code / `none (ocean)` / `code <v>` |
| `flow_accum` | ≤ 0 / > 0 | `0 (ocean)` / `<v> cells, A(v) km2 upstream` |
| `river` | 0 / other | `not a river` / `class <v>` |
| `lake`, `walk_area` | 0 / other | `none` / `id <v>` |
| `coast_dist` | < 0 / ≥ 0 | `<|v|> m offshore` / `<v> m from the sea` |
| `temperature` | any | `T(v) °C` |
| `rainfall` | any | `<v> mm/yr` |
| score layers | any | `<v> / 255` |
| `biome` | any | `code <v>` (the name needs table `biome`: the caller adds it) |
| `region` | 0 / other | `none (ocean)` / `id <v>` |
| any other name | any | `<v>` |

No other function formats a value for display; the legend labels of 4.4 use these.

### 4.4 Legends

```
struct LegendEntry { std::string label; std::vector<std::uint32_t> colours; };  // 0xRRGGBB; one = swatch, several = strip
std::vector<LegendEntry> layer_legend(std::string_view name, Palette palette, const db::LayerGrid& grid,
                                      std::span<const BiomeRecord> biomes);
std::vector<LegendEntry> composite_legend(Composite, std::span<const BiomeRecord> biomes);
std::string_view composite_description(Composite) noexcept;
```

Rows and order are exactly those of the viewer's `layer_catalogue.cpp` at `origin/main` (`fdd7b2f`), which this section restates so that the test writer can work from it alone. `pack(c)` = `(r << 16) | (g << 8) | b`; colours from the functions and stops of render.hpp.

| palette | rows |
| --- | --- |
| hypsometric | one row per `HYPSOMETRIC_LAND_STOPS` stop, `T(t) m`; then one per `HYPSOMETRIC_OCEAN_STOPS` stop, `sea, T(t) m deep` |
| temperature | per `TEMPERATURE_STOPS` stop, `T(t) °C` |
| rainfall | per `RAINFALL_STOPS` stop, `<t> mm/yr` |
| score | per `SCORE_STOPS` stop, `<t> / 255`, with ` (none)` at 0 and ` (best)` at 255 |
| water | v = 0..3: `format_layer_value("water", v)`, `water_colour(v)` |
| river | `0 not a river` (`river_colour(0)`); `class k` for k = 1..5; `class 6 to 15` (`river_colour(6)`) |
| direction | d = 0..8: `<d> <direction text of d>` (as `flow_dir` of 4.3), `direction_colour(d)` |
| log2 | v in 0, 1, 16, 256, 4096, 65536, 1048576: `<v>`, or for `flow_accum` and v > 0 `<v> cells (A(v) km2)`; `log2_colour(v)` |
| ids | `0: none` (`ids_colour(0)`); `each id its own colour` with the strip `ids_colour(1..5)` |
| signed | with (P, M) = `signed_scale(grid)`, v in −M, −M/2 (C++ truncation), −1, 0, P/2, P, skipping any outside [−M, P]: `format_layer_value(name, v)`, `signed_colour(v)` |
| biome | every record of `biomes` in the given order: `name` (or `key` when `name` is empty), `colour & 0xFFFFFF` |
| gray | none for an invalid grid; else min row `<min text> (min)` gray 0, an unlabelled strip gray 0, 128, 255, max row `<max text> (max)` gray `gray_level(r, r)`; min and max texts by `format_layer_value(name, ·)` (u64 above INT64_MAX as the plain decimal) |

| composite | `composite_description` | legend rows |
| --- | --- | --- |
| `map` | `relief shaded by slope` | the hypsometric land rows; then the water rows W |
| `biomes` | `biome colours shaded by slope` | biome rows of records with `water` = 0; then W |
| `regions` | `biome colours shaded by slope, region borders` | `region border` (0x000000); biome rows with `water` = 0; then W |

W = `ocean, 0 to 3000 m deep` with the strip of `HYPSOMETRIC_OCEAN_STOPS` colours; `lake` (64, 128, 204); `river, class 1 to 6+` with the strip `composite_river_colour(1..6)`.

### 4.5 Composites available for a world

```
std::vector<Composite> available_composites(std::span<const db::LayerInfo> layers, bool biome_table_usable);
```

In `ALL_COMPOSITES` order: none without a layer `elevation`; `map` with it; `biomes` also needs a layer `biome` and `biome_table_usable`; `regions` also needs those and a layer `region`. Names only, no decoding: a composite listed here can still fail to draw (its `composite_*_problem` text is the error, as today). This is the viewer's `WorldFile::composites` rule, moved. `worldgen-cli render` is not changed by it (it is told which composite to draw).

### 4.6 Region names

`RegionRecord` (geography.hpp) gains two fields, after `id`: `std::string name` (column 2 of P1-06 3.2) and `std::int64_t biome` (column 3). `read_region_rows` fills them; its checks and messages are unchanged (it already checks that `name` is text and `biome` an integer). `report` reads neither, so no report line changes. `biome` is added because the extension's `get_region_entries` exposes it today and would otherwise keep a raw read.

```
std::string_view region_name(std::span<const RegionRecord> records, std::int64_t id) noexcept;
```

The name of the record with that id, or empty when there is none (id 0, unknown id, empty span). Records in ascending id, as `read_region_rows` returns them (primary-key order); binary search. Unordered records give an unspecified record or empty, never undefined behaviour.

History keeps its own reading of names (`db::read_region_rows`, `HistoryView`); nothing there changes.

## 5. A layer the catalogue does not know

A world from a newer generator is normally refused for its schema version (P4-00 9). Within one schema version a new layer needs no bump (P1-00 4 rule 3: readers ignore layers they do not know), so an unknown name is a normal case, not an error:

- `find_layer_spec` returns null; `layer_group` gives `Other`; `layer_unit` empty; `registered_palette` gray; `format_layer_value` the plain decimal; `layer_legend` the rows of the palette it is given (for gray: min and max as plain decimals).
- A known name whose stored type differs from the catalogue type gets the entry's group, unit and display form; `auto_palette` already falls back to gray when the registered palette does not suit the grid, and the legend follows the palette actually used.
- No function in this header throws for any name, any int64 value or any valid grid (`signed_scale` is only called for the `signed` palette, which `auto_palette` gives only to a signed grid; `layer_legend` with `signed` on an unsigned or invalid grid returns no rows instead of raising; the viewer's copy would have raised there, a case its auto palette never reaches). The viewer shows such a layer under Other with its raw value, as today.

## 6. Changes in the extension (behaviour-preserving swap)

Done by the viewer engineer after the library part merges (P4-00 3.2), as its own small branch or with P4-03, not held for later. Every string the viewer shows is unchanged.

1. Delete `gdext/src/viewer/layer_catalogue.hpp` and `.cpp`; drop the source from `gdext/CMakeLists.txt`.
2. `viewer_model.cpp`: `catalogue::group_title`, `layer_group`, `layer_unit`, `format_value`, `cell_value`, `layer_legend`, `composite_legend` become the `worldgen::inspect` functions of 4.1 to 4.4 (`format_value` → `format_layer_value`). `get_legend_title` for a composite becomes `<name>: <composite_description>` (same three strings as now); for a layer it is unchanged (`<name>: <unit>`, or `<name> (palette <p>)` when the unit is empty). `legend_array` stays (Godot conversion).
3. `viewer_screen.cpp`: the group loop uses `LAYER_GROUP_COUNT` and `layer_group_title`.
4. `world_file.cpp` / `.hpp`: `read_region_table` keeps the `RegionRecord`s of `read_region_rows` (now with name and biome) instead of building `RegionRow` from columns 1, 2 and 10; the constants `NAME`, `BIOME`, `CELLS` and struct `RegionRow` go; `get_region_name` calls `region_name`; `get_region_entries` builds its dictionaries from the records (same keys `id`, `name`, `biome`, `cells`, same order). `composites()` calls `available_composites(layers_, biome_table_problem_.is_empty())`.
5. `viewer_check.cpp`: `catalogue::cell_value` and `catalogue::format_value` become the library functions; every expected string and every assertion message is unchanged (the five unit strings of the check, `123.4 m`, `-1.5 °C`, `lake`, `none (ocean)`, `48 m offshore`, stay literal).
6. `gdext/README.md`: the paragraph on `layer_catalogue.hpp` says the viewer takes groups, units, display forms, legends, available composites and region names from `worldgen/inspect/layer_catalogue.hpp` and `RegionRecord`; the "Classes" table is unchanged.

Nothing else in `gdext/` or `game/` changes. No SQL is added to the extension.

## 7. Invariants

1. `layer_catalogue()` has 19 entries, names unique, in P1-00 3's layer id order; each entry's type, range, scale and palette match P1-00 4 and section 4.2's table.
2. For every layer of a world generated with every stage (and with each `--until` stage), `find_layer_spec` is non-null, its `type` equals the stored element type, and every cell value lies in [min, max] (and ≤ width × height for `flow_accum`). Every catalogue entry is present in such a world (no stale entry).
3. `registered_palette(n)` equals the mapping documented in render.hpp before this change, for every catalogue name and for unknown names.
4. Every function of the header is total over its inputs as stated in 5: no throw, no undefined behaviour.
5. Every text is a pure function of its arguments: integer arithmetic only, no locale, no floating point, no unordered container; bytes are the same on every platform and compiler.
6. `region_name(records, r.id) == r.name` for every record read from a world; empty for 0 and for an id above the largest.
7. `available_composites` equals the viewer's current rule for every combination of the three layers and the flag (8 cases with `elevation`, plus without).

## 8. Determinism notes

The catalogue does not reach a world file. It reaches output text in two ways: `registered_palette` (the `--palette auto` choice of `render`, whose pixels are a cross-platform contract) and, in later stages, text a `report` or CLI listing may print (P4-00 3.1 leaves this to P4-02; see question 1: none in this stage). So it is held to the inspection code's rules: the determinism lint scope covers it (`worldgen/src/inspect/`), decimals come from `db::format_i64` / `format_u64` (not `std::to_string`, `printf` or streams, which bring locale and width questions), the table is a fixed array in a defined order, and lookups are linear or binary searches over that array, never a hash map.

**For the determinism auditor:** (a) `registered_palette` gives the same result as before for every name (a changed palette changes `render` pixels and the headless check's images); (b) every formatting path is integer only, including the negative and INT64_MIN cases of `T(v)` and the 256 n overflow bound of `A(n)` (n ≤ u32 max for `flow_accum`; for u64 inputs of `log2` legends the values are the fixed list); (c) `°` is a `°` escape, so the bytes do not depend on the source encoding (MSVC also builds with `/utf-8`); (d) the `RegionRecord` change does not alter `report`'s `[geography]` region keys or `hash` lines; (e) no new SQL, no new lint suppression.

## 9. Tests required

Library, `worldgen-tests` (new file `worldgen/tests/inspect_layer_catalogue_test.cpp`, in `-other` unless the test writer gives it a group; the world-backed cases join the shard whose support header caches the world they use, recorded in the `unmapped_add_doctest_shards()` call):

1. The table: 19 entries in order, unique names, each field of 4.2's table; `description` non-empty, at most 160 bytes, ASCII plus `°`.
2. `registered_palette` and `find_layer_spec(...)->palette` for every name, compared with a literal copy of the mapping (not with each other); unknown names (`""`, `"Elevation"`, `"water_level"`) give gray, Other, empty unit, null.
3. `format_layer_value`: a literal table of at least two cases per row of 4.3, including every special value, negatives (`elevation -5` → `-0.5 m (sea)`, `temperature -15` → `-1.5 °C`), 0, the type limits, INT64_MIN and INT64_MAX for an unknown layer, `flow_accum 1` → `1 cells, 0.00 km2 upstream`, `flow_accum 4096` → `4096 cells, 0.10 km2 upstream`.
4. `cell_value`: inside, outside, each element type, a u64 above INT64_MAX.
5. `layer_legend`: for each palette, labels and colours against literal expected lists written from section 4.4; gray on signed, unsigned and constant grids and an invalid grid; signed with M or P small enough that a row is skipped; `signed` on an unsigned grid returns no rows; biome with a record without name.
6. `composite_legend` and `composite_description` for the three composites with a two-record fixture biome table (one water, one land).
7. `available_composites`: all cases of invariant 7.
8. `read_region_rows`: name and biome filled; every existing problem message unchanged; `region_name` for present, absent, 0 and empty-span cases.
9. Against generated worlds (invariant 2): the shipped 512 world of seed 42 and a `--until hydrology` world (or the cached worlds of the existing support headers at those stages); and the test-pattern world, whose layers all give null (Other).

The test writer writes the expected strings and colours from this spec, independently of the implementation; the viewer's `layer_catalogue.cpp` at `fdd7b2f` is the behaviour being preserved and may be used to check the spec, not as the oracle.

Unchanged and required to pass: every existing `worldgen-tests` and `worldgen-cli-tests` entry (render and report outputs unchanged), the determinism lint and its self-test, `world-hashes.sh local` across presets with every golden file unchanged.

Extension: `gdext-headless-check` passes with the same assertion set; the validation specialist compares its log lines before and after the swap (they must be identical apart from paths and times), on `gdext-linux` and `gdext-windows`. No new assertions (P4-00 7).

No golden file is added or changed.

## 10. Open questions

1. **Show units in `worldgen-cli`?** P4-00 3.1 lets P4-02 decide whether `render --help` or a new `report` listing shows the units. Recommendation and interim: **neither in this stage**. A `report` change is a format change of the cross-platform text, and `render --help` would grow by 19 lines for little use. If Paul wants it, a later small change adds `worldgen-cli layers` (one line per catalogue entry: name, type, range, unit) with a P0-04 amendment; the catalogue is ready for it.
2. **Catalogue as code, not TOML** (section 3). Recommendation and interim: code. Proposals entry in appendix A.
3. **`RegionRecord` gains `biome` as well as `name`** (4.6), so that the extension's region entries need no raw read. Reading taken where P4-00 named only `name`; no design question.
4. **Descriptions are not shown yet.** The viewer's tooltip and legend title keep using `unit`; P4-03 or later may show `description` in the layer tooltip. Interim: not shown, so that this stage changes nothing visible.

## Appendix A. Proposals entry (added with this spec)

- "The layer catalogue is code, not TOML data" (section 3).
