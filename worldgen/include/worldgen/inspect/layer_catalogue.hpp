#pragma once

// The layer catalogue of spec P1-00 section 4 as code (spec P4-02): each
// layer's group, type, value range, display unit and form, special values
// and registered palette; values in display units; the legend of every
// palette and composite; the composites a world can show; region names.
//
// One definition for every reader: worldgen-cli render (registered_palette),
// the viewer and, later, the create-world screen and the in-game map. Plain
// C++20, no engine dependency. Every text is integer arithmetic only, with
// decimals from db::format_i64 / format_u64 (no locale, no floating point),
// so the bytes are the same on every platform and compiler (P4-02 8).
//
// A layer the catalogue does not know is a normal case (P1-00 4 rule 3):
// find_layer_spec gives nullptr, layer_group Other, layer_unit empty,
// format_layer_value the plain decimal (P4-02 5). No function here throws for
// any name, any int64 value or any grid, valid or not (allocation failure
// aside).

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "worldgen/db/layer_blob.hpp"
#include "worldgen/db/reader.hpp"
#include "worldgen/inspect/geography.hpp"
#include "worldgen/inspect/render.hpp"

namespace worldgen::inspect {

// --- Groups (P4-02 4.1) --------------------------------------------------------

// The groups of a layer list, in list order.
enum class LayerGroup : std::uint8_t {
    composites = 0,
    terrain = 1,
    water = 2,
    climate = 3,
    biomes = 4,
    resources = 5,
    regions = 6,
    other = 7,
};

inline constexpr std::size_t LAYER_GROUP_COUNT = 8;

// "Composites", "Terrain", "Water", "Climate", "Biomes", "Resources",
// "Regions", "Other"; an out-of-range enumerator value gives "Other".
[[nodiscard]] std::string_view layer_group_title(LayerGroup group) noexcept;

// --- Catalogue entries (P4-02 4.2) ---------------------------------------------

// A value of a layer with its own meaning ("0 none").
struct SpecialValue {
    std::int64_t value = 0;
    std::string_view label;
};

// One layer of P1-00 section 4.
struct LayerSpec {
    std::string_view name;
    LayerGroup group = LayerGroup::other;
    db::ElementType type = db::ElementType::u8;  // the catalogue type of P1-00 4
    std::int64_t min = 0;                         // value range of P1-00 4
    std::int64_t max = 0;
    bool max_is_cell_count = false;               // true: the real maximum is width * height (flow_accum)
    std::int64_t scale = 1;                       // decimal divisor to display units: 1 or 10
    std::string_view display_unit;                // "m", "°C", "mm/yr", "/ 255", or ""
    std::string_view unit;                        // unit and meaning in a few words (the viewer's legend title)
    std::string_view description;                 // one sentence, from P1-00 4
    std::span<const SpecialValue> special;        // ascending value
    Palette palette = Palette::gray;              // the registered palette (--palette auto)
};

// The 19 entries, in layer id order (P1-00 3).
[[nodiscard]] std::span<const LayerSpec> layer_catalogue() noexcept;

// The entry of a layer name (exact case), or nullptr when unknown.
[[nodiscard]] const LayerSpec* find_layer_spec(std::string_view name) noexcept;

// The entry's group, or Other when unknown.
[[nodiscard]] LayerGroup layer_group(std::string_view name) noexcept;

// The entry's unit, or empty when unknown.
[[nodiscard]] std::string_view layer_unit(std::string_view name) noexcept;

// --- Values in display units (P4-02 4.3) ---------------------------------------

// The value of cell (x, y) as int64; false outside the grid, for an index
// past the value vector, and for a u64 value above INT64_MAX (the caller
// shows the raw decimal instead).
[[nodiscard]] bool cell_value(const db::LayerGrid& grid, std::uint32_t x, std::uint32_t y,
                              std::int64_t& value) noexcept;

// The value v of a layer in display units ("123.4 m", "-1.5 °C", "lake",
// "142 / 255"); the plain decimal for a layer the catalogue does not know.
// Biome and region names are not added (they need the world's tables).
[[nodiscard]] std::string format_layer_value(std::string_view name, std::int64_t v);

// --- Legends (P4-02 4.4) ----------------------------------------------------------

// One row of a legend: a label and one colour (a swatch) or several (a strip,
// left to right), each 0xRRGGBB.
struct LegendEntry {
    std::string label;
    std::vector<std::uint32_t> colours;
};

// The legend of a layer drawn with palette (normally auto_palette(name,
// grid)). grid gives the value range for gray and signed; biomes the world's
// table `biome` for the biome palette. Gray on an invalid grid and signed on
// an invalid or unsigned grid give no rows.
[[nodiscard]] std::vector<LegendEntry> layer_legend(std::string_view name, Palette palette,
                                                    const db::LayerGrid& grid,
                                                    std::span<const BiomeRecord> biomes);

// The legend of a composite (P1-00 10.2, P1-04 9, P0-04 amendment 18 (e)).
[[nodiscard]] std::vector<LegendEntry> composite_legend(Composite composite,
                                                        std::span<const BiomeRecord> biomes);

// "relief shaded by slope", "biome colours shaded by slope", "biome colours
// shaded by slope, region borders"; empty for an out-of-range value.
[[nodiscard]] std::string_view composite_description(Composite composite) noexcept;

// --- Composites available for a world (P4-02 4.5) --------------------------------

// In ALL_COMPOSITES order: none without a layer `elevation`; map with it;
// biomes also needs a layer `biome` and biome_table_usable; regions also
// needs those and a layer `region`. Names only, no decoding.
[[nodiscard]] std::vector<Composite> available_composites(std::span<const db::LayerInfo> layers,
                                                          bool biome_table_usable);

// --- Region names (P4-02 4.6) -----------------------------------------------------

// The name of the record with that id, or empty when there is none (id 0,
// unknown id, empty span). records in ascending id, as read_region_rows
// returns them; binary search. Unordered records give an unspecified record's
// name or empty, never undefined behaviour.
[[nodiscard]] std::string_view region_name(std::span<const RegionRecord> records, std::int64_t id) noexcept;

}  // namespace worldgen::inspect
